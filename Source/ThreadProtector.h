#pragma once
#include "Assert.h"


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


	// RAII wrapper for ThreadProtector
	class ScopedThreadProtector
	{
	public:
		ScopedThreadProtector(ThreadProtector& threadProtector) : m_tpObj(threadProtector) { m_tpObj.TakeOwnership(); }
		~ScopedThreadProtector() { m_tpObj.ReleaseOwnership(); }

	private:
		ThreadProtector& m_tpObj;

	private: // No copying allowed
		ScopedThreadProtector() = delete;
		ScopedThreadProtector(ScopedThreadProtector const&) = delete;
		ScopedThreadProtector(ScopedThreadProtector&&) = delete;
		ScopedThreadProtector& operator=(ScopedThreadProtector const&) = delete;
		ScopedThreadProtector& operator=(ScopedThreadProtector&&) = delete;
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

			SEAssert("Recursive TakeOwnership() call detected", m_owningThreadId != std::this_thread::get_id());
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