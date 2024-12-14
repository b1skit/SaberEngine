// © 2023 Adam Badke. All rights reserved.
#pragma once


namespace util
{
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


	inline uint64_t HashString(std::string const& str)
	{
		return std::hash<std::string>{}(str);
	}


	inline uint64_t HashDataBytes(void const* const data, size_t numBytes)
	{
		uint64_t dataHash = 0;

		constexpr uint8_t k_wordSize = sizeof(uint64_t); // 8 bytes in a word on 64-bit architecture

		const uint64_t numWords = numBytes / k_wordSize;
		const uint64_t remainingNumBytes = numBytes - (numWords * k_wordSize);

		uint64_t const* wordPtr = static_cast<uint64_t const*>(data);
		for (size_t curWord = 0; curWord < numWords; curWord++)
		{
			util::AddDataToHash(dataHash, *wordPtr);
			wordPtr++;
		}

		// Pack the remaining bytes into a single word, with any remaining bytes padded with 0's
		uint64_t remainingBytes = 0;
		uint8_t const* bytePtr = static_cast<uint8_t const*>(data) + (numWords * k_wordSize);
		memcpy(&remainingBytes, bytePtr, remainingNumBytes);

		util::AddDataToHash(dataHash, remainingBytes);

		return dataHash;
	}


	template<typename T>
	void AddDataBytesToHash(uint64_t& currentHash, T const& data)
	{
		CombineHash(currentHash, HashDataBytes(&data, sizeof(T)));
	}


	// DataHash is a wrapper around a uint64_t, with some convenience helpers that allows the hash value to be used as
	// a key in an associative array without being re-hashed
	struct DataHash
	{
		uint64_t m_dataHash;

		DataHash() noexcept : m_dataHash(0) {}

		DataHash(uint64_t hash) noexcept : m_dataHash(hash) {}

		DataHash(DataHash const&) noexcept = default;
		DataHash(DataHash&&) noexcept = default;

		~DataHash() = default;

		DataHash& operator=(DataHash const&) noexcept = default;
		DataHash& operator=(DataHash&&) noexcept = default;

		template<typename T>
		DataHash& operator=(T const& hashVal) noexcept { m_dataHash = hashVal; return *this; };

		template<typename T>
		operator T& () noexcept { return m_dataHash; }

		bool operator==(DataHash const& rhs) const noexcept { return m_dataHash == rhs.m_dataHash; }
		bool operator<(DataHash const& rhs) const noexcept { return m_dataHash < rhs.m_dataHash; }
		bool operator>(DataHash const& rhs) const noexcept { return m_dataHash > rhs.m_dataHash; }

		bool operator()(DataHash const& lhs, DataHash const& rhs) const noexcept { return lhs.m_dataHash == rhs.m_dataHash; }
		bool operator()(DataHash const& rhs) const noexcept { return m_dataHash == rhs.m_dataHash; }
	};
}


// Hash functions for our util::DataHash, to allow it to be used as a key in an associative container
template<>
struct std::hash<util::DataHash>
{
	std::size_t operator()(util::DataHash const& dataHash) const noexcept
	{
		return dataHash.m_dataHash;
	}
};


template<>
struct std::hash<util::DataHash const>
{
	std::size_t operator()(util::DataHash const& dataHash) const noexcept
	{
		return dataHash.m_dataHash;
	}
};


template <>
struct std::formatter<util::DataHash>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	template <typename FormatContext>
	auto format(const util::DataHash& dataHash, FormatContext& ctx)
	{
		return std::format_to(ctx.out(), "{}", dataHash.m_dataHash);
	}
};