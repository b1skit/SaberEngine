// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "HashUtils.h"

#include "../Assert.h"


namespace util
{
	// HashKey is a wrapper around a uint64_t, with some convenience helpers that allows the hash value to be used as
	// a key in an associative array without being re-hashed
	struct HashKey final
	{
		uint64_t m_hashKey;

		HashKey() noexcept : m_hashKey(0) {}

		HashKey(uint64_t hash) noexcept : m_hashKey(hash) {}
		HashKey(int zeroInit) noexcept : m_hashKey(zeroInit) { SEAssert(m_hashKey == 0, "Unexpected data width"); }

		HashKey(char const* const cStr) noexcept : m_hashKey(util::HashString(cStr)) {}
		HashKey(std::string const& str) noexcept : HashKey(str.c_str()) {}

		HashKey(HashKey const&) noexcept = default;
		HashKey(HashKey&&) noexcept = default;

		~HashKey() = default;

		HashKey& operator=(HashKey const&) noexcept = default;
		HashKey& operator=(HashKey&&) noexcept = default;

		template<typename T>
		HashKey& operator=(T const& hashVal) noexcept { m_hashKey = hashVal; return *this; };

		template<typename T>
		operator T& () noexcept { return m_hashKey; }

		template<typename T>
		operator T () const noexcept { return m_hashKey; }

		bool operator==(HashKey const& rhs) const noexcept { return m_hashKey == rhs.m_hashKey; }
		bool operator==(uint64_t rhs) const noexcept { return m_hashKey == rhs; }
		bool operator==(int rhs) const noexcept { SEAssert(rhs == 0, "Unexpected comparison"); return m_hashKey == rhs; } // Convenience only

		bool operator<(HashKey const& rhs) const noexcept { return m_hashKey < rhs.m_hashKey; }
		bool operator>(HashKey const& rhs) const noexcept { return m_hashKey > rhs.m_hashKey; }

		bool operator()(HashKey const& lhs, HashKey const& rhs) const noexcept { return lhs.m_hashKey == rhs.m_hashKey; }
		bool operator()(HashKey const& rhs) const noexcept { return m_hashKey == rhs.m_hashKey; }
	};
}


// Hash functions for our util::HashKey, to allow it to be used as a key in an associative container
template<>
struct std::hash<util::HashKey>
{
	std::size_t operator()(util::HashKey const& hashKey) const noexcept
	{
		return hashKey.m_hashKey;
	}
};


template<>
struct std::hash<util::HashKey const>
{
	std::size_t operator()(util::HashKey const& hashKey) const noexcept
	{
		return hashKey.m_hashKey;
	}
};


template <>
struct std::formatter<util::HashKey>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}

	template <typename FormatContext>
	auto format(util::HashKey const& hashKey, FormatContext& ctx) const
	{
		return std::format_to(ctx.out(), "{}", hashKey.m_hashKey);
	}
};