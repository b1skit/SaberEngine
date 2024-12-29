// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Config.h"
#include "LogManager.h"
#include "ThreadPool.h"

#include "Util/CastUtils.h"


namespace core
{
	FunctionWrapper& FunctionWrapper::operator=(FunctionWrapper&& other) noexcept
	{
		m_impl = std::move(other.m_impl);
		return *this;
	}


	ThreadPool* ThreadPool::Get()
	{
		static std::unique_ptr<core::ThreadPool> instance = std::make_unique<core::ThreadPool>();
		return instance.get();
	}


	ThreadPool::ThreadPool()
		: m_minThreadCount(0)
		, m_isRunning(false)
	{
	}


	void ThreadPool::Startup()
	{
		m_minThreadCount = std::thread::hardware_concurrency();
		SEAssert(m_minThreadCount > 0, "Failed to query the number of threads supported");
		LOG("System has %d logical threads", m_minThreadCount);

		size_t actualNumThreads = m_minThreadCount;
		if (Config::Get()->KeyExists(configkeys::k_minWorkerThreads))
		{
			actualNumThreads = util::CheckedCast<size_t>(Config::Get()->GetValue<int>(configkeys::k_minWorkerThreads));
		}		

		m_isRunning = true; // Must be true BEFORE a new thread checks this in ExecuteJobs()

		for (size_t i = 0; i < actualNumThreads; ++i)
		{
			AddWorkerThread();
		}
	}


	void ThreadPool::Stop()
	{
		{
			std::unique_lock<std::mutex> waitingLock(m_jobQueueMutex);

			m_isRunning = false;
			waitingLock.unlock();
			m_jobQueueCV.notify_all();
		}

		{
			std::lock_guard<std::shared_mutex> lock(m_workerThreadsMutex);

			for (auto& thread : m_workerThreads)
			{
				m_workerThreads.at(thread.first).join();
			}
			m_workerThreads.clear();

			while (!m_workerThreadsToJoin.empty())
			{
				m_workerThreadsToJoin.front().join();
			}
			m_workerThreadsToJoin.clear();
		}
	}


	void ThreadPool::ExecuteJobs()
	{
		while (m_isRunning)
		{
			// Aquire the lock and get a job, waiting if no jobs exist:
			std::unique_lock<std::mutex> waitingLock(m_jobQueueMutex);
			m_jobQueueCV.wait(waitingLock, 
				[this](){ return !m_jobQueue.empty() || !m_isRunning;} // False if waiting should continue
			);
			if (!m_isRunning)
			{
				return;
			}

			// Get the job from the queue:
			FunctionWrapper currentJob = std::move(m_jobQueue.front());
			m_jobQueue.pop();

			const size_t currentJobQueueSize = m_jobQueue.size();

			waitingLock.unlock();

			currentJob(); // Do the work

			if (ShrinkThreadPool(currentJobQueueSize, std::this_thread::get_id()))
			{
				return;
			}
		}
	}


	void ThreadPool::NameCurrentThread(wchar_t const* threadName)
	{
		const HRESULT hr = ::SetThreadDescription(
			::GetCurrentThread(),
			threadName);

		SEAssert(hr >= 0, "Failed to set thread name");
	}


	void ThreadPool::GrowThreadPool(size_t currentJobQueueSize)
	{
		size_t numWorkerThreads = 0;
		{
			std::shared_lock<std::shared_mutex> readLock(m_workerThreadsMutex);

			numWorkerThreads = m_workerThreads.size();
		}

		// Grow the pool by adding 1 thread for each additional k_targetJobsPerThread in the queue
		const size_t flooredSize = (currentJobQueueSize / k_targetJobsPerThread) * k_targetJobsPerThread;
		if (flooredSize > (numWorkerThreads * k_targetJobsPerThread))
		{
			AddWorkerThread();
		}
	}


	bool ThreadPool::ShrinkThreadPool(size_t currentJobQueueSize, std::thread::id currentThread)
	{
		bool shouldTerminate = false;
		{
			std::unique_lock<std::shared_mutex> lock(m_workerThreadsMutex);

			if (currentJobQueueSize < m_minThreadCount &&
				m_workerThreads.size() > m_minThreadCount) // Never shrink smaller than our target
			{
				SEAssert(m_workerThreads.contains(currentThread), "Failed to find the current thread");

				m_workerThreadsToJoin.emplace_back(std::move(m_workerThreads.at(currentThread)));
				m_workerThreads.erase(currentThread);

				shouldTerminate = true;

				LOG(std::format("Thread pool shrunk to {} threads", m_workerThreads.size()).c_str());
			}

			// Threads can't join() themselves, clean up any previously-released threads
			while (!m_workerThreadsToJoin.empty() &&
				m_workerThreadsToJoin.front().get_id() != currentThread)
			{
				m_workerThreadsToJoin.front().join();
				m_workerThreadsToJoin.pop_front();
			}
		}

		return shouldTerminate;
	}


	void ThreadPool::AddWorkerThread()
	{
		{
			std::lock_guard<std::shared_mutex> lock(m_workerThreadsMutex);

			std::thread newThread = std::thread(&ThreadPool::ExecuteJobs, this);
			m_workerThreads.emplace(newThread.get_id(), std::move(newThread));

			// Set this as a default, it can be overriden at any point
			NameCurrentThread(L"Worker Thread");

			LOG(std::format("Thread pool grown to {} threads", m_workerThreads.size()).c_str());
		}
	}
}