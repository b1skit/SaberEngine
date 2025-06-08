// © 2024 Adam Badke. All rights reserved.
#ifndef SE_TARGET_PARAMS
#define SE_TARGET_PARAMS

#include "Private/PlatformConversions.h"


struct TargetData
{
	float4 g_targetDims; // .x = width, .y = height, .z = 1/width, .w = 1/height

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "TargetParams";
#endif
};


#endif // SE_TARGET_PARAMS