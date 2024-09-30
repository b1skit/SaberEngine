// © 2024 Adam Badke. All rights reserved.
#pragma once
#include "HashUtils.h"

#include "../Assert.h"


namespace util
{
	// Hash of a std::string - Convenience/efficiency wrapper for associative containers
	class StringHash
	{
	public:
		static constexpr uint64_t k_invalidNameHash = std::numeric_limits<uint64_t>::max();

	public:
		StringHash(std::string const& name);

		StringHash() : m_nameHash(k_invalidNameHash) {}; // Invalid

		~StringHash() = default;
		StringHash(StringHash const&) = default;
		StringHash(StringHash&&) noexcept = default;
		StringHash& operator=(StringHash const&) = default;
		StringHash& operator=(StringHash&&) noexcept = default;

		bool operator==(StringHash const& rhs) const { return m_nameHash == rhs.m_nameHash; }
		bool operator<(StringHash const& rhs) const { return m_nameHash < rhs.m_nameHash; }
		bool operator>(StringHash const& rhs) const { return m_nameHash > rhs.m_nameHash; }

		bool operator()(StringHash const& lhs, StringHash const& rhs) const { return lhs.m_nameHash == rhs.m_nameHash; }
		bool operator()(StringHash const& rhs) const { return m_nameHash == rhs.m_nameHash; }

		uint64_t Get() const { return m_nameHash; }
		bool IsValid() const { return m_nameHash != k_invalidNameHash; }


	private:
		uint64_t m_nameHash;
	};


	inline StringHash::StringHash(std::string const& name)
		: m_nameHash(util::HashString(name))
	{
		SEAssert(m_nameHash != k_invalidNameHash, "Hash collides with invalid hash sentinel");
	}
}


// Hash functions for our StringHash, to allow it to be used as a key in an associative container
template<>
struct std::hash<util::StringHash>
{
	std::size_t operator()(util::StringHash const& nameHash) const
	{
		return nameHash.Get();
	}
};


template <>
struct std::formatter<util::StringHash>
{
	constexpr auto parse(std::format_parse_context& ctx)
	{
		return ctx.begin();
	}


	template <typename FormatContext>
	auto format(const util::StringHash& nameHash, FormatContext& ctx)
	{
		return std::format_to(ctx.out(), "{}", nameHash.Get());
	}
};