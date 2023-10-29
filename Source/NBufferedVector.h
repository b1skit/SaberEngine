// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "DebugConfiguration.h"


namespace util
{
	// An N-buffered std::vector wrapper.
	// Intended for consuming a frame's worth of data while the next frame's data is being recorded, with variable
	// length to accompodate data in flight. The oldest data is cleared when Swap() is called.
	template<typename T>
	class NBufferedVector
	{
	public:
		enum BufferSize : uint8_t
		{
			Two = 2,
			Three = 3
		};
	public:
		NBufferedVector(BufferSize, size_t reserveSize);

		~NBufferedVector();
		void Destroy();

		// Reads must be manually locked, as values are returned by reference. This guards against references being held
		// while a call to Swap() happens. Multiple readers can aquire the read lock at the same time
		void AquireReadLock();
		void ReleaseReadLock();

		T const& operator[](size_t index) const;
		std::vector<T> const& GetReadData() const;

		bool HasReadData() const;


		// Simultaneous writes are thread-safe. No need to manually obtain a lock
		void EmplaceBack(T&&); // std::vector::emplace_back
		void EmplaceBack(T const&);


		void Swap(); // Advance the read/write indexes, and clears the oldest buffer

		// Optional: Clear the data in the read buffer. Useful when you need to clear without calling Swap()
		void ClearReadData();


	private:
		const uint8_t m_numBuffers;
		std::unique_ptr<std::vector<T>[]> m_vectors; // N-buffer
		std::array<std::shared_mutex, 2> m_vectorMutexes; // Only need 2 mutexes to synchronize reading/writing

		uint8_t m_readIdx;	// Starts at 0, progresses by (m_readIdx + 1) % m_numBuffers each time Swap() is called
		uint8_t m_writeIdx; // Starts at 1, progresses by (m_writeIdx + 1) % m_numBuffers each time Swap() is called


#if defined(_DEBUG)
		// Validate that reads are thread safe in debug mode
		mutable std::set<uint64_t> m_readingThreads;
		mutable std::mutex m_readingThreadsMutex;
#endif
		void RegisterReadingThread() const;
		void UnregisterReadingThread() const;
		void AssertReadingLock(bool lockExpected) const;


	private:
		NBufferedVector() = delete;

		// No copying allowed
		NBufferedVector(NBufferedVector&) = delete;
		NBufferedVector& operator=(NBufferedVector const&) = delete;

		// We could allow thread-safe moves, but don't need them for now so haven't bothered
		NBufferedVector(NBufferedVector&&) = delete;
		NBufferedVector& operator=(NBufferedVector&&) = delete;
	};


	template<typename T>
	NBufferedVector<T>::NBufferedVector(BufferSize bufferSize, size_t reserveSize)
		: m_numBuffers(static_cast<uint8_t>(bufferSize))
		, m_readIdx(0)
		, m_writeIdx(1)
	{
		{
			std::scoped_lock lock(m_vectorMutexes[0], m_vectorMutexes[1]);

			m_vectors = std::make_unique<std::vector<T>[]>(m_numBuffers);

			for (uint8_t i = 0; i < m_numBuffers; i++)
			{
				m_vectors[i].reserve(reserveSize);
			}
		}
	}


	template<typename T>
	NBufferedVector<T>::~NBufferedVector()
	{
		Destroy();
	}


	template<typename T>
	void NBufferedVector<T>::Destroy()
	{
		std::scoped_lock lock(m_vectorMutexes[0], m_vectorMutexes[1]);
		m_vectors = nullptr;
	}



	template<typename T>
	void NBufferedVector<T>::Swap()
	{
		std::scoped_lock lock(m_vectorMutexes[0], m_vectorMutexes[1]);

		// Advance the write index to the oldest buffer, and clear it
		m_writeIdx = (m_writeIdx + 1) % m_numBuffers;
		m_vectors[m_writeIdx].clear();
		
		// Advance the read index to the most recently written
		m_readIdx = (m_readIdx + 1) % m_numBuffers;
	}


	template<typename T>
	void NBufferedVector<T>::ClearReadData()
	{
		{
			std::lock_guard<std::shared_mutex>lock(m_vectorMutexes[m_readIdx]);
			m_vectors[m_readIdx].clear();
		}
	}


	template<typename T>
	inline void NBufferedVector<T>::AquireReadLock()
	{
		m_vectorMutexes[m_readIdx].lock_shared();
		RegisterReadingThread();
	}


	template<typename T>
	inline void NBufferedVector<T>::ReleaseReadLock()
	{
		UnregisterReadingThread();
		m_vectorMutexes[m_readIdx].unlock_shared();
	}


	template<typename T>
	T const& NBufferedVector<T>::operator[](size_t index) const
	{
		SEAssert("Index is OOB", index < m_vectors[m_readIdx].size());

		AssertReadingLock(true);
		return m_vectors[m_readIdx][index];
	}


	template<typename T>
	std::vector<T> const& NBufferedVector<T>::GetReadData() const
	{
		AssertReadingLock(true);
		return m_vectors[m_readIdx];
	}


	template<typename T>
	bool NBufferedVector<T>::HasReadData() const
	{
		AssertReadingLock(true);
		return !m_vectors[m_readIdx].empty();
	}


	template<typename T>
	void NBufferedVector<T>::EmplaceBack(T&& value)
	{
		{
			std::lock_guard<std::shared_mutex> lock(m_vectorMutexes[m_writeIdx]);

			m_vectors[m_writeIdx].emplace_back(std::move(value));
		}
	}


	template<typename T>
	void NBufferedVector<T>::EmplaceBack(T const& value)
	{
		{
			std::lock_guard<std::mutex> lock(m_vectorMutexes[m_writeIdx]);

			m_vectors[m_writeIdx].emplace_back(value);
		}
	}


	template<typename T>
	void NBufferedVector<T>::RegisterReadingThread() const
	{
#if defined(_DEBUG)

		AssertReadingLock(false);

		{
			std::hash<std::thread::id> hasher;
			std::thread::id threadID = std::this_thread::get_id();
			const uint64_t threadIdHash = hasher(threadID);
			m_readingThreads.emplace(threadIdHash);
		}
#endif
	}


	template<typename T>
	void NBufferedVector<T>::UnregisterReadingThread() const
	{
#if defined(_DEBUG)

		AssertReadingLock(true);

		{
			std::hash<std::thread::id> hasher;
			std::thread::id threadID = std::this_thread::get_id();
			const uint64_t threadIdHash = hasher(threadID);
			m_readingThreads.erase(threadIdHash);
		}
#endif
	}


	template<typename T>
	void NBufferedVector<T>::AssertReadingLock(bool lockExpected) const
	{
#if defined(_DEBUG)
		{
			std::lock_guard<std::mutex> lock(m_readingThreadsMutex);
			
			std::hash<std::thread::id> hasher;
			std::thread::id threadID = std::this_thread::get_id();
			const uint64_t threadIdHash = hasher(threadID);

			SEAssert("Thread does not hold a reading lock",
				m_readingThreads.contains(threadIdHash) == lockExpected);
		}
#endif
	}
}