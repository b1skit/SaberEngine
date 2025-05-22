// © 2025 Adam Badke. All rights reserved.
#pragma once


namespace grutil
{
	inline uint32_t GetRoundedDispatchDimension(uint32_t totalDimension, uint32_t workGroupDimension)
	{
		return (totalDimension + (workGroupDimension - 1)) / workGroupDimension;
	}
}