// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"


namespace util
{
	// A double-buffered std::unordered_map wrapper.
	// Intended for consuming a single frame's worth of data while the next frame's data is being recorded.
	// Data is cleared when Swap() is called.
	template<class Key, class Value>
	class DoubleBufferUnorderedMap
	{
	public:		
		DoubleBufferUnorderedMap(size_t reserveSize);

		~DoubleBufferUnorderedMap() = default;
		void Destroy();

		// Reads must be manually locked, as values are returned by reference. This guards against references being held
		// after a call to Swap() has occured.
		void AquireReadLock();
		void ReleaseReadLock();

		Value const& Get(Key const&) const;
		std::unordered_map<Key, Value> const& Get() const;

		bool HasReadData() const;

		// The Simultaneous writes are thread-safe.
		void Set(Key const&, Value&&); // emplace

		void Swap();
		void EndOfFrame(); // Clear read buffers


	private:
		static constexpr uint8_t k_numBuffers = 2;
		std::array<std::unordered_map<Key, Value>, k_numBuffers> m_unorderedMaps;
		std::array<std::mutex, k_numBuffers> m_unorderedMapMutexes;
		
		std::atomic<std::thread::id> m_readingThread;

		uint8_t m_readIdx;
		uint8_t m_writeIdx;


	private:
		DoubleBufferUnorderedMap() = delete;

		// No copying allowed
		DoubleBufferUnorderedMap(DoubleBufferUnorderedMap&) = delete;
		DoubleBufferUnorderedMap& operator=(DoubleBufferUnorderedMap const&) = delete;
		
		// We could allow thread-safe moves, but don't need them for now so haven't bothered
		DoubleBufferUnorderedMap(DoubleBufferUnorderedMap&&) noexcept = delete;
		DoubleBufferUnorderedMap& operator=(DoubleBufferUnorderedMap&&) noexcept = delete;
	};


	template<class Key, class Value>
	DoubleBufferUnorderedMap<Key, Value>::DoubleBufferUnorderedMap(size_t reserveSize)
		: m_writeIdx(0)
		, m_readIdx(1)
		, m_readingThread(std::thread::id())
	{
		{
			std::scoped_lock lock(m_unorderedMapMutexes[0], m_unorderedMapMutexes[1]);

			m_unorderedMaps[0].reserve(reserveSize);
			m_unorderedMaps[1].reserve(reserveSize);
		}
	}


	template<class Key, class Value>
	void DoubleBufferUnorderedMap<Key, Value>::Destroy()
	{
		std::scoped_lock lock(m_unorderedMapMutexes[0], m_unorderedMapMutexes[1]);
		
		m_unorderedMaps[0].clear();
		m_unorderedMaps[1].clear();
	}


	template<class Key, class Value>
	void DoubleBufferUnorderedMap<Key, Value>::Swap()
	{
		// Avoid deadlocks: Lock both of our mutexes simultaneously:
		std::scoped_lock lock(m_unorderedMapMutexes[0], m_unorderedMapMutexes[1]);

		SEAssert("The read unordered_map should be empty. Did you forget to call EndOfFrame() before Swap()?",
			m_unorderedMaps[m_readIdx].empty());

		const uint8_t temp = m_readIdx;
		m_readIdx = m_writeIdx;
		m_writeIdx = temp;
	}


	template<class Key, class Value>
	void DoubleBufferUnorderedMap<Key, Value>::EndOfFrame()
	{
		AquireReadLock();
		m_unorderedMaps[m_readIdx].clear();
		ReleaseReadLock();
	}


	template<class Key, class Value>
	inline void DoubleBufferUnorderedMap<Key, Value>::AquireReadLock()
	{
		m_unorderedMapMutexes[m_readIdx].lock();
		m_readingThread = std::this_thread::get_id();
	}


	template<class Key, class Value>
	inline void DoubleBufferUnorderedMap<Key, Value>::ReleaseReadLock()
	{
		m_readingThread = std::thread::id();
		m_unorderedMapMutexes[m_readIdx].unlock();
	}


	template<class Key, class Value>
	Value const& DoubleBufferUnorderedMap<Key, Value>::Get(Key const& key) const
	{
		SEAssert(m_readingThread == std::this_thread::get_id(), "Thread is not holding the read lock");

		auto const& result = m_unorderedMaps[m_readIdx].find(key);
		SEAssert(result != m_unorderedMaps[m_readIdx].end(), "No result found for the given key");
		return result.second;
	}


	template<class Key, class Value>
	std::unordered_map<Key, Value> const& DoubleBufferUnorderedMap<Key, Value>::Get() const
	{
		SEAssert(m_readingThread == std::this_thread::get_id(), "Thread is not holding the read lock");
		return m_unorderedMaps[m_readIdx];
	}


	template<class Key, class Value>
	bool DoubleBufferUnorderedMap<Key, Value>::HasReadData() const
	{
		SEAssert(m_readingThread == std::this_thread::get_id(), "Thread is not holding the read lock");
		return !m_unorderedMaps[m_readIdx].empty();
	}


	template<class Key, class Value>
	void DoubleBufferUnorderedMap<Key, Value>::Set(Key const& key, Value&& value)
	{
		{
			std::lock_guard<std::mutex> lock(m_unorderedMapMutexes[m_writeIdx]);

			SEAssert("An object with this key already exists", 
				m_unorderedMaps[m_writeIdx].contains(key) == false);

			m_unorderedMaps[m_writeIdx].emplace(key, std::move(value));
		}
	}
}