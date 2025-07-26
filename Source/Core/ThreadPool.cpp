// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Config.h"
#include "ThreadPool.h"
#include "Logger.h"

#include "Util/CastUtils.h"


namespace core
{
	FunctionWrapper& FunctionWrapper::operator=(FunctionWrapper&& other) noexcept
	{
		m_impl = std::move(other.m_impl);
		return *this;
	}


	// ---


	bool ThreadPool::s_isRunning = false;

	std::mutex ThreadPool::s_jobQueueMutex;
	std::condition_variable ThreadPool::s_jobQueueCV;

	std::queue<FunctionWrapper> ThreadPool::s_jobQueue;

	std::vector<std::thread> ThreadPool::s_workerThreads;


	void ThreadPool::Startup()
	{
		const size_t numLogicalThreads = std::thread::hardware_concurrency();
		SEAssert(numLogicalThreads > 0, "Failed to query the number of threads supported");
		LOG("System has %d logical threads", numLogicalThreads);

		size_t actualNumThreads = numLogicalThreads;
		if (Config::KeyExists(configkeys::k_numWorkerThreads))
		{
			actualNumThreads = util::CheckedCast<size_t>(Config::GetValue<int>(configkeys::k_numWorkerThreads));
		}		

		s_isRunning = true; // Must be true BEFORE a new thread checks this in ExecuteJobs()

		for (size_t i = 0; i < actualNumThreads; ++i)
		{
			AddWorkerThread();
		}
	}


	void ThreadPool::Stop()
	{
		{
			std::unique_lock<std::mutex> waitingLock(s_jobQueueMutex);

			s_isRunning = false;			
		}
		s_jobQueueCV.notify_all();

		// Wait for all of our threads to complete:
		for (auto& thread : s_workerThreads)
		{
			thread.join();
		}
		s_workerThreads.clear();
	}


	void ThreadPool::ExecuteJobs()
	{
		while (s_isRunning)
		{
			// Aquire the lock and get a job, or wait if no jobs exist:
			std::unique_lock<std::mutex> waitingLock(s_jobQueueMutex);
			s_jobQueueCV.wait(
				waitingLock, 
				[](){ return !s_jobQueue.empty() || !s_isRunning;}); // False if waiting should continue
			
			if (!s_isRunning)
			{
				return;
			}

			// Get the job from the queue:
			FunctionWrapper currentJob = std::move(s_jobQueue.front());
			s_jobQueue.pop();

			waitingLock.unlock();

			currentJob(); // Do the work
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
		s_workerThreads.emplace_back(std::thread(&ThreadPool::ExecuteJobs));

		const HRESULT hr = ::SetThreadDescription(
			s_workerThreads.back().native_handle(),
			L"Worker Thread");

		SEAssert(hr >= 0, "Failed to set thread name");
	}
}