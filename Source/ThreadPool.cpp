// © 2022 Adam Badke. All rights reserved.
#include "ThreadPool.h"
#include "DebugConfiguration.h"


namespace en
{
	FunctionWrapper& FunctionWrapper::operator=(FunctionWrapper&& other)
	{
		m_impl = std::move(other.m_impl);
		return *this;
	}


	ThreadPool::ThreadPool()
		: m_maxThreads(0)
		, m_isRunning(false)
	{
	}


	void ThreadPool::Startup()
	{
		m_maxThreads = std::thread::hardware_concurrency();
		SEAssert("Failed to query the number of threads supported", m_maxThreads > 0);
		LOG("System has %d logical threads", m_maxThreads);

		// Leave a couple of a thread spare for the OS
		const size_t actualNumThreads = m_maxThreads - 1;

		m_isRunning = true; // Must be true BEFORE a new thread checks this in ExecuteJobs()

		m_workerThreads.reserve(actualNumThreads);
		for (size_t i = 0; i < actualNumThreads; i++)
		{
			m_workerThreads.emplace_back(std::thread(&ThreadPool::ExecuteJobs, this));

			// Set this as a default, it can be overriden at any point
			NameCurrentThread(L"Worker Thread");
		}
	}


	void ThreadPool::Stop()
	{
		LOG("ThreadPool stopping...");

		std::unique_lock<std::mutex> waitingLock(m_jobQueueMutex);
		m_isRunning = false;
		waitingLock.unlock();
		m_jobQueueCV.notify_all();

		for (size_t i = 0; i < m_workerThreads.size(); i++)
		{
			m_workerThreads[i].join();
		}
		m_workerThreads.clear();
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

			waitingLock.unlock();

			// Finally, do the work:
			currentJob();
		}
	}


	void ThreadPool::NameCurrentThread(wchar_t const* threadName)
	{
		const HRESULT hr = ::SetThreadDescription(
			::GetCurrentThread(),
			threadName);

		SEAssert("Failed to set thread name", hr >= 0);
	}
}