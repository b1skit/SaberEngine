// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "StringHash.h"


namespace util
{
	// DataHash is a wrapper around a uint64_t, with some convenience helpers that allows the hash value to be used as
	// a key in an associative array without being re-hashed
	struct DataHash
	{
		uint64_t m_dataHash;

		DataHash() noexcept : m_dataHash(0) {}

		DataHash(uint64_t hash) noexcept : m_dataHash(hash) {}
		DataHash(int zeroInit) noexcept : m_dataHash(zeroInit) { SEAssert(m_dataHash == 0, "Unexpected data width"); }

		DataHash(util::StringHash const& stringHash) noexcept : m_dataHash(stringHash.Get()) {}
		DataHash(char const* const cStr) noexcept : m_dataHash(util::StringHash(cStr).Get()) {}
		DataHash(std::string const& str) noexcept : DataHash(str.c_str()) {}

		DataHash(DataHash const&) noexcept = default;
		DataHash(DataHash&&) noexcept = default;

		~DataHash() = default;

		DataHash& operator=(DataHash const&) noexcept = default;
		DataHash& operator=(DataHash&&) noexcept = default;

		template<typename T>
		DataHash& operator=(T const& hashVal) noexcept { m_dataHash = hashVal; return *this; };

		template<typename T>
		operator T& () noexcept { return m_dataHash; }

		template<typename T>
		operator T () const noexcept { return m_dataHash; }

		bool operator==(DataHash const& rhs) const noexcept { return m_dataHash == rhs.m_dataHash; }
		bool operator==(uint64_t rhs) const noexcept { return m_dataHash == rhs; }

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