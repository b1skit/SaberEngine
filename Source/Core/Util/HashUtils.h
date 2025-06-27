// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace util
{
	inline uint64_t HashString(std::string const& str)
	{
		return std::hash<std::string>{}(str);
	}


	inline uint64_t HashCStr(char const* cStr) // Constructs a temporary std::string, avoid this if you can
	{
		return HashString(cStr);
	}


	inline void CombineHash(uint64_t& currentHash, const uint64_t newHash)
	{
		// Lifted from Boost hash_combine + the 64-bit suggestions for the magic number & shift distances on this page:
		// https://github.com/HowardHinnant/hash_append/issues/7
		currentHash ^= newHash + 0x9e3779b97f4a7c15 + (currentHash << 12) + (currentHash >> 4);
	}


	inline void AddDataToHash(uint64_t& currentHash, const uint64_t dataVal)
	{
		static const std::hash<uint64_t> hasher;
		CombineHash(currentHash, hasher(dataVal));
	}


	inline uint64_t HashDataBytes(std::span<const std::byte> data)
	{
		uint64_t dataHash = 0;

		constexpr uint8_t k_wordSize = sizeof(uint64_t); // 8 bytes in a word on 64-bit architecture

		const uint64_t numWords = data.size() / k_wordSize;
		const uint64_t remainingNumBytes = data.size() - (numWords * k_wordSize);

		uint64_t const* wordPtr = reinterpret_cast<uint64_t const*>(data.data());
		for (size_t curWord = 0; curWord < numWords; curWord++)
		{
			util::AddDataToHash(dataHash, *wordPtr);
			wordPtr++;
		}

		// Pack the remaining bytes into a single word, with any remaining bytes padded with 0's
		uint64_t remainingBytes = 0;
		uint8_t const* bytePtr = reinterpret_cast<uint8_t const*>(data.data()) + (numWords * k_wordSize);
		memcpy(&remainingBytes, bytePtr, remainingNumBytes);

		util::AddDataToHash(dataHash, remainingBytes);

		return dataHash;
	}

	// Legacy overload for compatibility
	inline uint64_t HashDataBytes(void const* const data, size_t numBytes)
	{
		return HashDataBytes(std::span<const std::byte>{static_cast<const std::byte*>(data), numBytes});
	}


	inline void AddDataBytesToHash(uint64_t& currentHash, char const* cStr) // Constructs a temporary std::string, avoid this if you can
	{
		CombineHash(currentHash, HashCStr(cStr));
	}


	inline void AddDataBytesToHash(uint64_t& currentHash, std::string const& str)
	{
		CombineHash(currentHash, HashString(str));
	}


	template<typename T>
	void AddDataBytesToHash(uint64_t& currentHash, T const& data)
	{
		CombineHash(currentHash, HashDataBytes(&data, sizeof(T)));
	}
}