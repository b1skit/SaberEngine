// © 2024 Adam Badke. All rights reserved.
#pragma once

namespace util
{
	class CHashKey final
	{
	public:
		consteval CHashKey(char const* keyStr)
			: m_key(keyStr)
			, m_keyHash(Fnv1A(keyStr))
		{
		}


	public: // Dynamic creation, for when consteval isn't possible
		static CHashKey Create(char const* keyStr)
		{
			return CHashKey(keyStr, PrivateCTORAccessor{});
		}

		static CHashKey Create(std::string const& keyStr)
		{
			return CHashKey(keyStr.c_str(), PrivateCTORAccessor{});
		}


	public:
		CHashKey(CHashKey const& rhs) noexcept = default;	
		CHashKey& operator=(CHashKey const& rhs) noexcept = default;

		CHashKey(CHashKey&& rhs) noexcept = default;
		CHashKey& operator=(CHashKey&& rhs) noexcept = default;

		~CHashKey() = default;

		bool operator==(CHashKey const& rhs) const noexcept { return m_keyHash == rhs.m_keyHash; }
		bool operator<(CHashKey const& rhs) const noexcept { return m_keyHash < rhs.m_keyHash; }
		bool operator>(CHashKey const& rhs) const noexcept { return m_keyHash > rhs.m_keyHash; }

		bool operator()(CHashKey const& lhs, CHashKey const& rhs) const noexcept { return lhs == rhs; }
		bool operator()(CHashKey const& rhs) const noexcept { return m_keyHash == rhs.m_keyHash; }

		constexpr operator uint64_t() const { return m_keyHash; }


	public:
		constexpr char const* GetKey() const { return m_key; }
		constexpr uint64_t GetHash() const { return m_keyHash; }


	private:
		// Hash using the FNV-1a alternative algorithm: http://isthe.com/chongo/tech/comp/fnv/#FNV-1a

		constexpr uint64_t Fnv1A(uint64_t hash, const char* keyStr)
		{
			constexpr uint64_t k_fnvPrime = 1099511628211ull;
			return (*keyStr == 0) ? hash : Fnv1A((hash ^ static_cast<uint64_t>(*keyStr)) * k_fnvPrime, keyStr + 1);
		}


		constexpr uint64_t Fnv1A(const char* keyStr)
		{
			constexpr uint64_t k_fnvOffsetBasis = 14695981039346656037ull;
			return Fnv1A(k_fnvOffsetBasis, keyStr);
		}


	private:
		struct PrivateCTORAccessor {};
		CHashKey(char const* keyStr, PrivateCTORAccessor)
			: m_key(nullptr) // No dynamic allocations allowed
			, m_keyHash(Fnv1A(keyStr))
		{
		}


	private:
		char const* m_key;
		uint64_t m_keyHash;
	};
}


// Hash functions for our util::CHashKey, to allow it to be used as a key in an associative container
template<>
struct std::hash<util::CHashKey>
{
	std::size_t operator()(util::CHashKey const& hashKey) const noexcept
	{
		return hashKey.GetHash();
	}
};


template<>
struct std::hash<util::CHashKey const>
{
	std::size_t operator()(util::CHashKey const& hashKey) const noexcept
	{
		return hashKey.GetHash();
	}
};