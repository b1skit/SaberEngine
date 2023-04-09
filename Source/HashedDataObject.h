// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "DebugConfiguration.h"


namespace en
{
	class HashedDataObject
	{
	protected:
		virtual void ComputeDataHash() = 0; // Should be called once the implementer is fully initialized

	public:
		HashedDataObject() : m_dataHash(0) {}

	public:
		inline uint64_t GetDataHash() const;

		inline void AddDataBytesToHash(void const* const data, size_t numBytes);

		template<typename T>
		inline void AddDataBytesToHash(T const& data);

	private:
		uint64_t m_dataHash;
	};


	uint64_t HashedDataObject::GetDataHash() const
	{
		return m_dataHash;
	}


	void HashedDataObject::AddDataBytesToHash(void const* const data, size_t numBytes)
	{
		SEAssert("Invalid data for hash", data != nullptr && numBytes > 0);

		std::hash<uint8_t> hasher;
		uint8_t const* dataPtr = (uint8_t const*)data;
		for (size_t curByte = 0; curByte < numBytes; curByte++) // Lifted from Boost hash_combine:
		{
			m_dataHash ^= hasher(*dataPtr) + 0x9e3779b9 + (m_dataHash << 6) + (m_dataHash >> 2);
			dataPtr++;
		}
	}


	template<typename T>
	void HashedDataObject::AddDataBytesToHash(T const& data)
	{
		AddDataBytesToHash(&data, sizeof(T));
	}
}