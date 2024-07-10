// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "Core/Assert.h"


using DataHash = uint64_t;

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
		SEAssert(!str.empty(), "Cannot hash an empty string");
		return std::hash<std::string>{}(str);
	}
}