// © 2024 Adam Badke. All rights reserved.
#pragma once

namespace util
{
	class HashKey
	{
	public:
		consteval explicit HashKey(char const* keyStr)
			: m_key(keyStr)
			, m_keyHash(Fnv1A(keyStr))
		{
		}

		HashKey(HashKey const& rhs) noexcept = default;	
		HashKey& operator=(HashKey const& rhs) noexcept = default;

		HashKey(HashKey&& rhs) noexcept = default;
		HashKey& operator=(HashKey&& rhs) noexcept = default;

		~HashKey() = default;

		bool operator==(HashKey const& rhs) const { return m_keyHash == rhs.m_keyHash; }
		bool operator<(HashKey const& rhs) const { return m_keyHash < rhs.m_keyHash; }
		bool operator>(HashKey const& rhs) const { return m_keyHash > rhs.m_keyHash; }

		bool operator()(HashKey const& lhs, HashKey const& rhs) const { return lhs == rhs; }
		bool operator()(HashKey const& rhs) const { return m_keyHash == rhs.m_keyHash; }


	public:
		char const* GetKey() const { return m_key; }
		uint64_t GetHash() const { return m_keyHash; }


	private:
		// Hash using the FNV-1a alternative algorithm: http://isthe.com/chongo/tech/comp/fnv/#FNV-1a

		consteval uint64_t Fnv1A(uint64_t hash, const char* keyStr)
		{
			constexpr uint64_t k_fnvPrime = 1099511628211ull;
			return (*keyStr == 0) ? hash : Fnv1A((hash ^ static_cast<uint64_t>(*keyStr)) * k_fnvPrime, keyStr + 1);
		}


		consteval uint64_t Fnv1A(const char* keyStr)
		{
			constexpr uint64_t k_fnvOffsetBasis = 14695981039346656037ull;
			return Fnv1A(k_fnvOffsetBasis, keyStr);
		}


	private:
		char const* m_key;
		uint64_t m_keyHash;
	};
}


// Hash functions for our util::HashKey, to allow it to be used as a key in an associative container
template<>
struct std::hash<util::HashKey const>
{
	std::size_t operator()(util::HashKey const& hashKey) const
	{
		return hashKey.GetHash();
	}
};