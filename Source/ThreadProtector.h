#pragma once
#include "Assert.h"


namespace util
{
	class ThreadProtector
	{
	public:
		ThreadProtector(bool accessIsValidIfNotCurrentlyOwned);
		~ThreadProtector() = default;

		void TakeOwnership();
		void ValidateThreadAccess() const;
		void ReleaseOwnership();

#if defined(_DEBUG)
	private:
		std::thread::id m_owningThreadId;
		mutable std::mutex m_owningThreadIdMutex;

		// True: You only care about catching accesses while someone else has ownership of the ThreadProtector
		// False: You must own the ThreadProtector for access to be valid
		bool m_accessIsValidIfNotCurrentlyOwned;
#endif

	private: // No copying allowed
		ThreadProtector() = delete;
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


	inline ThreadProtector::ThreadProtector(bool accessIsValidIfNotCurrentlyOwned)
#if defined(_DEBUG)
		: m_accessIsValidIfNotCurrentlyOwned(accessIsValidIfNotCurrentlyOwned)
#endif
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

			SEAssert(m_owningThreadId != std::this_thread::get_id(), "Recursive TakeOwnership() call detected");
			SEAssert(m_owningThreadId == std::thread::id(), "ThreadProtector is already owned");

			m_owningThreadId = std::this_thread::get_id();
		}
#endif
	}

	
	inline void ThreadProtector::ValidateThreadAccess() const
	{
#if defined(_DEBUG)
		{
			std::lock_guard<std::mutex> lock(m_owningThreadIdMutex);

			SEAssert(m_owningThreadId == std::this_thread::get_id() || 
				(m_owningThreadId == std::thread::id() && m_accessIsValidIfNotCurrentlyOwned),
				"Thread access violation");
		}
#endif
	}


	inline void ThreadProtector::ReleaseOwnership()
	{
#if defined(_DEBUG)
		{
			std::lock_guard<std::mutex> lock(m_owningThreadIdMutex);

			SEAssert(m_owningThreadId != std::thread::id(), "Ownership of the ThreadProtector has not been claimed");
			SEAssert(m_owningThreadId == std::this_thread::get_id(), 
				"Non-owning thread is trying to release ownership of ThreadProtector");

			m_owningThreadId = std::thread::id();
		}
#endif
	}
}