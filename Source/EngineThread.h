// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace en
{
	class EngineThread
	{
	public:
		enum class SyncType
		{
			ReleaseWorker,		// The executing worker thread
			ReleaseCommander,	// Orchestrating master thread			

			SyncType_Count
		};


	public:
		EngineThread();
		~EngineThread() = default;


	public:
		virtual void Lifetime(std::barrier<>* copyBarrier) = 0;
		
		void ThreadStartup(); // Blocking
		void ThreadInitialize(); // Blocking
		void ThreadStop(); // Non-blocking: Signals the thread to exit the update loop
		void ThreadShutdown(); // Blocking


	public:
		struct ThreadUpdateParams
		{
			uint64_t m_frameNum;
			double m_elapsed;
		};
		void EnqueueUpdate(ThreadUpdateParams const& update);


	protected:
		std::queue<ThreadUpdateParams> m_updates;
		std::mutex m_updatesMutex;
		std::condition_variable m_updatesCV;

		bool GetUpdateParams(EngineThread::ThreadUpdateParams& updateParams);


	protected:
		std::array<std::latch, static_cast<size_t>(SyncType::SyncType_Count)> m_startupLatch;
		std::array<std::latch, static_cast<size_t>(SyncType::SyncType_Count)> m_initializeLatch;
		std::array<std::latch, static_cast<size_t>(SyncType::SyncType_Count)> m_shutdownLatch;

		std::atomic<bool> m_isRunning;


	private:
		EngineThread(EngineThread const&) = delete;
		EngineThread(EngineThread&&) = delete;
		EngineThread& operator=(EngineThread const&) = delete;
	};
}