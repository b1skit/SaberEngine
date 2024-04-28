// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace en
{
	class IEngineThread
	{
	public:
		enum class SyncType
		{
			ReleaseWorker,		// The executing worker thread
			ReleaseCommander,	// Orchestrating master thread			

			SyncType_Count
		};


	public:
		IEngineThread();
		IEngineThread(IEngineThread&&) = default;
		IEngineThread& operator=(IEngineThread&&) = default;
		~IEngineThread() = default;


	public:
		virtual void Lifetime(std::barrier<>* copyBarrier) = 0;
		
		void ThreadStartup(); // Blocking
		void ThreadInitialize(); // Blocking
		void ThreadStop(); // Non-blocking: Signals the thread to exit the update loop
		void ThreadShutdown(); // Blocking


	public:
		struct ThreadUpdateParams
		{
			uint64_t m_frameNum = 0;
			double m_elapsed = 0.0;
		};
		void EnqueueUpdate(ThreadUpdateParams const& update);


	protected:
		std::queue<ThreadUpdateParams> m_updates;
		std::mutex m_updatesMutex;
		std::condition_variable m_updatesCV;

		bool GetUpdateParams(IEngineThread::ThreadUpdateParams& updateParams);


	protected:
		std::array<std::latch, static_cast<size_t>(SyncType::SyncType_Count)> m_startupLatch;
		std::array<std::latch, static_cast<size_t>(SyncType::SyncType_Count)> m_initializeLatch;
		std::array<std::latch, static_cast<size_t>(SyncType::SyncType_Count)> m_shutdownLatch;

		std::atomic<bool> m_isRunning;


	private:
		IEngineThread(IEngineThread const&) = delete;
		IEngineThread& operator=(IEngineThread const&) = delete;
	};
}