// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"


namespace util
{
	template<typename T>
	class ThreadSafeVector
	{
	public:
		ThreadSafeVector() = default;
		ThreadSafeVector(ThreadSafeVector const&);
		ThreadSafeVector(ThreadSafeVector&&) noexcept;
		
		ThreadSafeVector(std::vector<T> const&);
		ThreadSafeVector(std::vector<T>&&) noexcept;

		ThreadSafeVector& operator=(ThreadSafeVector const&);
		ThreadSafeVector& operator=(ThreadSafeVector&&) noexcept;

		~ThreadSafeVector();

		bool empty() const;
		size_t size() const;
		void reserve(size_t);

		void clear();

		void emplace_back(T&&);

		// Note: Accessing elements in the vector is not thread safe (e.g. a race might occur if accessing an element
		// that is modified by another thread, or if the object is moved due to an internal resize etc)
		T const& operator[](size_t idx) const;
		T const& at(size_t pos) const;


	private:
		std::vector<T> m_vector;
		mutable std::mutex m_vectorMutex;
	};


	template<typename T>
	inline ThreadSafeVector<T>::ThreadSafeVector(ThreadSafeVector const& rhs)
	{
		std::unique_lock<std::mutex> lock(m_vectorMutex);
		std::unique_lock<std::mutex> rhsLock(rhs.m_vectorMutex);
		m_vector = rhs.m_vector;
	}


	template<typename T>
	inline ThreadSafeVector<T>::ThreadSafeVector(ThreadSafeVector&& rhs) noexcept
	{
		std::unique_lock<std::mutex> lock(m_vectorMutex);
		std::unique_lock<std::mutex> rhsLock(rhs.m_vectorMutex);
		m_vector = std::move(rhs.m_vector);
	}


	template<typename T>
	inline ThreadSafeVector<T>::ThreadSafeVector(std::vector<T> const& rhs)
	{
		std::unique_lock<std::mutex> lock(m_vectorMutex);
		m_vector = rhs;
	}


	template<typename T>
	inline ThreadSafeVector<T>::ThreadSafeVector(std::vector<T>&& rhs) noexcept
	{
		std::unique_lock<std::mutex> lock(m_vectorMutex);
		m_vector = std::move(rhs);
	}


	template<typename T>
	inline ThreadSafeVector<T>& ThreadSafeVector<T>::operator=(ThreadSafeVector const& rhs)
	{
		if (&rhs == this)
		{
			return *this;
		}
		std::unique_lock<std::mutex> lock(m_vectorMutex);
		std::unique_lock<std::mutex> rhsLock(rhs.m_vectorMutex);
		m_vector = rhs.m_vector;
	}


	template<typename T>
	inline ThreadSafeVector<T>& ThreadSafeVector<T>::operator=(ThreadSafeVector&& rhs) noexcept
	{
		if (&rhs == this)
		{
			return *this;
		}
		std::unique_lock<std::mutex> lock(m_vectorMutex);
		std::unique_lock<std::mutex> rhsLock(rhs.m_vectorMutex);
		m_vector = std::move(rhs.m_vector);
		return *this;
	}


	template<typename T>
	inline ThreadSafeVector<T>::~ThreadSafeVector()
	{
		std::unique_lock<std::mutex> lock(m_vectorMutex);
		m_vector.clear();
	}


	template<typename T>
	inline bool ThreadSafeVector<T>::empty() const
	{
		return m_vector.empty();
	}


	template<typename T>
	inline size_t ThreadSafeVector<T>::size() const
	{
		return m_vector.size();
	}


	template<typename T>
	inline void ThreadSafeVector<T>::reserve(size_t reserveSize)
	{
		std::unique_lock<std::mutex> lock(m_vectorMutex);
		m_vector.reserve(reserveSize);
	}


	template<typename T>
	inline void ThreadSafeVector<T>::clear()
	{
		std::unique_lock<std::mutex> lock(m_vectorMutex);
		m_vector.clear();
	}


	template<typename T>
	inline void ThreadSafeVector<T>::emplace_back(T&& newVal)
	{
		std::unique_lock<std::mutex> lock(m_vectorMutex);
		m_vector.emplace_back(std::move(newVal));
	}


	template<typename T>
	inline T const& ThreadSafeVector<T>::operator[](size_t idx) const
	{
		return m_vector[idx];
	}


	template<typename T>
	inline T const& ThreadSafeVector<T>::at(size_t idx) const
	{
		return m_vector.at(idx);
	}
}