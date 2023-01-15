// � 2022 Adam Badke. All rights reserved.
#include "EngineThread.h"


namespace
{
	constexpr size_t k_shutdownFrameNumSignal = static_cast<size_t>(-1);
}

namespace en
{
	EngineThread::EngineThread()
		: m_isRunning(false)
		, m_startupLatch{std::latch((size_t)SyncType::SyncType_Count), std::latch((size_t)SyncType::SyncType_Count)}
		, m_initializeLatch{std::latch((size_t)SyncType::SyncType_Count), std::latch((size_t)SyncType::SyncType_Count)}
		, m_shutdownLatch{std::latch((size_t)SyncType::SyncType_Count), std::latch((size_t)SyncType::SyncType_Count)}
	{
	}


	void EngineThread::ThreadStartup()
	{
		m_startupLatch[static_cast<size_t>(SyncType::ReleaseWorker)].arrive_and_wait();
		m_startupLatch[static_cast<size_t>(SyncType::ReleaseCommander)].arrive_and_wait();
	}


	void EngineThread::ThreadInitialize()
	{
		m_initializeLatch[static_cast<size_t>(SyncType::ReleaseWorker)].arrive_and_wait();
		m_initializeLatch[static_cast<size_t>(SyncType::ReleaseCommander)].arrive_and_wait();
	}


	void EngineThread::ThreadStop()
	{
		m_isRunning.store(false);
	}


	void EngineThread::ThreadShutdown()
	{
		// Pack a shutdown signal into the update queue
		EnqueueUpdate({k_shutdownFrameNumSignal, 0.f});

		m_shutdownLatch[static_cast<size_t>(SyncType::ReleaseWorker)].arrive_and_wait();
		m_shutdownLatch[static_cast<size_t>(SyncType::ReleaseCommander)].arrive_and_wait();
	}


	void EngineThread::EnqueueUpdate(ThreadUpdateParams const& update)
	{
		{
			std::lock_guard<std::mutex> lock(m_updatesMutex);
			m_updates.emplace(update);
		}
		m_updatesCV.notify_one();
	}


	bool EngineThread::GetUpdateParams(EngineThread::ThreadUpdateParams& updateParams)
	{
		std::unique_lock<std::mutex> waitLock(m_updatesMutex);
		m_updatesCV.wait(waitLock, 
			[this]() {return !m_updates.empty() || !m_isRunning; }); // True stops the wait

		if (!m_isRunning)
		{
			return false;
		}

		updateParams = m_updates.front();
		m_updates.pop();
		
		return updateParams.m_frameNum != k_shutdownFrameNumSignal;
	}
}