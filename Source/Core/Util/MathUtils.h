// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"


namespace util
{
	template<typename T>
	T RoundUpToNearestMultiple(T val, T multiple)
	{
		SEAssert(val >= 0 && multiple > 0, "Invalid values. Val must be non-negative, multiple must be non-zero");
		
		const T remainder = val % multiple;
		if (remainder == 0)
		{
			return val > 0 ? val : multiple;
		}
		return val + multiple - remainder;
	}


	template<typename T>
	bool IsPowerOfTwo(T v)
	{
		// Bit twiddling hack: http://www.graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
		return v && !(v & (v - 1));
	}
}