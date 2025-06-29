// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Config.h"
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
		: m_isRunning(false)
	{
	}


	void ThreadPool::Startup()
	{
		const size_t numLogicalThreads = std::thread::hardware_concurrency();
		SEAssert(numLogicalThreads > 0, "Failed to query the number of threads supported");
		LOG("System has %d logical threads", numLogicalThreads);

		size_t actualNumThreads = numLogicalThreads;
		if (Config::Get()->KeyExists(configkeys::k_numWorkerThreads))
		{
			actualNumThreads = util::CheckedCast<size_t>(Config::Get()->GetValue<int>(configkeys::k_numWorkerThreads));
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
		}
		m_jobQueueCV.notify_all();

		// Wait for all of our threads to complete:
		for (auto& thread : m_workerThreads)
		{
			thread.join();
		}
		m_workerThreads.clear();
	}


	void ThreadPool::ExecuteJobs()
	{
		while (m_isRunning)
		{
			// Aquire the lock and get a job, or wait if no jobs exist:
			std::unique_lock<std::mutex> waitingLock(m_jobQueueMutex);
			m_jobQueueCV.wait(
				waitingLock, 
				[this](){ return !m_jobQueue.empty() || !m_isRunning;}); // False if waiting should continue
			
			if (!m_isRunning)
			{
				return;
			}

			// Get the job from the queue:
			FunctionWrapper currentJob = std::move(m_jobQueue.front());
			m_jobQueue.pop();

			waitingLock.unlock();

			// Execute the job with exception handling
			try
			{
				currentJob(); // Do the work
			}
			catch (...)
			{
				SEAssert(false, "ThreadPool job threw an exception");
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


	void ThreadPool::AddWorkerThread()
	{
		m_workerThreads.emplace_back(std::thread(&ThreadPool::ExecuteJobs, this));

		const HRESULT hr = ::SetThreadDescription(
			m_workerThreads.back().native_handle(),
			L"Worker Thread");

		SEAssert(hr >= 0, "Failed to set thread name");
	}
}