#pragma once
#include "DebugConfiguration.h"


namespace util
{
	class ThreadProtector
	{
	public:
		ThreadProtector();
		~ThreadProtector() = default;

		void TakeOwnership();
		void ValidateThreadAccess() const;
		void ReleaseOwnership();

	private:
		std::thread::id m_owningThreadId;
		mutable std::mutex m_owningThreadIdMutex;


	private: // No copying allowed
		ThreadProtector(ThreadProtector const&) = delete;
		ThreadProtector(ThreadProtector&&) = delete;
		ThreadProtector& operator=(ThreadProtector const&) = delete;
		ThreadProtector& operator=(ThreadProtector&&) = delete;
	};


	inline ThreadProtector::ThreadProtector()
	{
#if defined(_DEBUG)
		{
			std::lock_guard<std::mutex> lock(m_owningThreadIdMutex);
			m_owningThreadId = std::thread::id();
		}
#endif
	}


	inline void ThreadProtector::TakeOwnership()
	{
#if defined(_DEBUG)
		{
			std::lock_guard<std::mutex> lock(m_owningThreadIdMutex);

			SEAssert("ThreadProtector is already owned", m_owningThreadId == std::thread::id());

			m_owningThreadId = std::this_thread::get_id();
		}
#endif
	}

	
	inline void ThreadProtector::ValidateThreadAccess() const
	{
#if defined(_DEBUG)
		{
			std::lock_guard<std::mutex> lock(m_owningThreadIdMutex);

			SEAssert("Thread access violation", 
				m_owningThreadId == std::this_thread::get_id() || 
				m_owningThreadId == std::thread::id());
		}
#endif
	}


	inline void ThreadProtector::ReleaseOwnership()
	{
#if defined(_DEBUG)
		{
			std::lock_guard<std::mutex> lock(m_owningThreadIdMutex);

			SEAssert("Ownership of the ThreadProtector has not been claimed", m_owningThreadId != std::thread::id());
			SEAssert("Non-owning thread is trying to release ownership of ThreadProtector", 
				m_owningThreadId == std::this_thread::get_id());

			m_owningThreadId = std::thread::id();
		}
#endif
	}
}