// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace en
{
	class ThreadPool
	{
	public:
		ThreadPool();
		ThreadPool(ThreadPool&&) = default;
		ThreadPool& operator=(ThreadPool&&) = default;
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
		ThreadPool& operator=(ThreadPool const&) = delete;		
	};
}