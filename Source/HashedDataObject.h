// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "DebugConfiguration.h"
#include "HashUtils.h"


namespace en
{
	class HashedDataObject
	{
	protected:
		virtual void ComputeDataHash() = 0; // Should be called once the implementer is fully initialized

	public:
		HashedDataObject() : m_dataHash(0) {}

	public:
		uint64_t GetDataHash() const;

		void AddDataBytesToHash(void const* const data, size_t numBytes);

		void AddDataBytesToHash(std::string const& str);

		template<typename T>
		void AddDataBytesToHash(T const& data);

		template<typename T>
		void AddDataBytesToHash(std::vector<T> const& dataVec);

		void ResetDataHash();

	private:
		uint64_t m_dataHash;
	};


	inline uint64_t HashedDataObject::GetDataHash() const
	{
		return m_dataHash;
	}


	inline void HashedDataObject::AddDataBytesToHash(void const* const data, size_t numBytes)
	{
		SEAssert("Invalid data for hash", data != nullptr && numBytes > 0);

		uint8_t const* dataPtr = (uint8_t const*)data;
		for (size_t curByte = 0; curByte < numBytes; curByte++)
		{
			util::AddDataToHash(m_dataHash, *dataPtr);
			dataPtr++;
		}
		// TODO: This could be done more efficiently by stepping through the data in 64-bit chunks (and handling any
		// remaining bytes at the end)
	}


	inline void HashedDataObject::AddDataBytesToHash(std::string const& str)
	{
		AddDataBytesToHash(str.c_str(), str.size());
	}


	template<typename T>
	inline void HashedDataObject::AddDataBytesToHash(T const& data)
	{
		AddDataBytesToHash(&data, sizeof(T));
	}


	template<typename T>
	inline void HashedDataObject::AddDataBytesToHash(std::vector<T> const& dataVec)
	{
		for (T const& t : dataVec)
		{
			AddDataBytesToHash(t);
		}
	}


	inline void HashedDataObject::ResetDataHash()
	{
		m_dataHash = 0;
	}
}