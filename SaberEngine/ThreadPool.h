#pragma once

#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>


namespace en
{
	class ThreadPool
	{
	public:
		ThreadPool();
		~ThreadPool() = default;

		void Startup();
		void Stop();

		void EnqueueJob(std::function<void()> const& job); // Producer


	private:
		void ExecuteJobs(); // Consumer loop

	private:
		uint8_t m_maxThreads;
		bool m_isRunning;

		std::mutex m_jobQueueMutex;
		std::condition_variable m_jobQueueCV;

		std::queue<std::function<void()>> m_jobQueue;

		std::vector<std::thread> m_workerThreads;


	private:
		// No moving or copying allowed
		ThreadPool(ThreadPool const&) = delete;
		ThreadPool(ThreadPool&&) = delete;
		ThreadPool& operator=(ThreadPool const&) = delete;
		ThreadPool& operator=(ThreadPool&&) = delete;
	};
}