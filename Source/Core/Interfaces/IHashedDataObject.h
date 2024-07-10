// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/Assert.h"
#include "Core/Util/HashUtils.h"


namespace core
{
	class IHashedDataObject
	{
	protected:
		virtual void ComputeDataHash() = 0; // Should be called once the implementer is fully initialized

	public:
		IHashedDataObject() : m_dataHash(0) {}

	public:
		DataHash GetDataHash() const;

		void AddDataBytesToHash(void const* const data, size_t numBytes);

		void AddDataBytesToHash(std::string const& str);

		template<typename T>
		void AddDataBytesToHash(T const& data);

		template<typename T>
		void AddDataBytesToHash(std::vector<T> const& dataVec);

		void ResetDataHash();

	private:
		DataHash m_dataHash;
	};


	inline DataHash IHashedDataObject::GetDataHash() const
	{
		return m_dataHash;
	}


	inline void IHashedDataObject::AddDataBytesToHash(void const* const data, size_t numBytes)
	{
		SEAssert(data != nullptr && numBytes > 0, "Invalid data for hash");

		constexpr uint8_t k_wordSize = sizeof(uint64_t); // 8 bytes in a word on 64-bit architecture
		const uint64_t numWords = numBytes / k_wordSize;
		const uint64_t remainingNumBytes = numBytes - (numWords * k_wordSize);

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


	inline void IHashedDataObject::AddDataBytesToHash(std::string const& str)
	{
		AddDataBytesToHash(std::hash<std::string>{}(str));
	}


	template<typename T>
	inline void IHashedDataObject::AddDataBytesToHash(T const& data)
	{
		AddDataBytesToHash(&data, sizeof(T));
	}


	template<typename T>
	inline void IHashedDataObject::AddDataBytesToHash(std::vector<T> const& dataVec)
	{
		for (T const& t : dataVec)
		{
			AddDataBytesToHash(t);
		}
	}


	inline void IHashedDataObject::ResetDataHash()
	{
		m_dataHash = 0;
	}
}