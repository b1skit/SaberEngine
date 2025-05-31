// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Assert.h"


namespace util
{
	template<typename T>
	bool IsPowerOfTwo(T v)
	{
		// Bit twiddling hack: http://www.graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
		return v && !(v & (v - 1));
	}


	template<typename T>
	T RoundUpToNearestMultiple(T val, T multiple)
	{
		SEAssert(val >= 0 && multiple > 0, "Invalid values. Val must be non-negative, multiple must be non-zero");

		// Check if multiple is a power of two. The IsPowerOfTwo function is in the same namespace and file.
		if (IsPowerOfTwo<T>(multiple))
		{
			// Bitwise trick for power-of-two multiples
			// Ensure T(1) is used for type consistency in subtraction,
			// especially if T might be a larger type than int.
			return (val + multiple - T(1)) & ~(multiple - T(1));
		}
		else
		{
			const T remainder = val % multiple;
			if (remainder == 0)
			{
				return val;
			}
			return val + multiple - remainder;
		}
	}


	template<typename T>
	T DivideAndRoundUp(T val, T divisor)
	{
		// Rounds the remainder up to the nearest integer, and includes it in the result
		return (val + divisor - T(1)) / divisor;
	}
}