// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "DebugConfiguration.h"


namespace util
{
	template<typename T>
	T RoundUpToNearestMultiple(T val, T multiple)
	{
		SEAssert("Invalid values. Val must be non-negative, multiple must be non-zero", val >= 0 && multiple > 0);
		
		const T remainder = val % multiple;
		if (remainder == 0)
		{
			return val;
		}
		return val + multiple - remainder;
	}
}