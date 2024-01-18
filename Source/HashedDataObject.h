// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Assert.h"
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
		SEAssert(data != nullptr && numBytes > 0, "Invalid data for hash");

		constexpr uint8_t k_wordSize = sizeof(uint64_t); // 8 bytes in a word on 64-bit architecture
		const size_t numWords = numBytes / k_wordSize;
		const size_t remainingNumBytes = numBytes - (numWords * k_wordSize);

		uint64_t const* wordPtr = static_cast<uint64_t const*>(data);
		for (size_t curWord = 0; curWord < numWords; curWord++)
		{
			util::AddDataToHash(m_dataHash, *wordPtr);
			wordPtr++;
		}

		// Pack the remaining bytes into a single word, with any remaining bytes padded with 0's
		uint64_t remainingBytes = 0;
		uint8_t const* bytePtr = static_cast<uint8_t const*>(data) + (numWords * k_wordSize);
		memcpy(&remainingBytes, bytePtr, remainingNumBytes);

		util::AddDataToHash(m_dataHash, remainingBytes);
	}


	inline void HashedDataObject::AddDataBytesToHash(std::string const& str)
	{
		AddDataBytesToHash(std::hash<std::string>{}(str));
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