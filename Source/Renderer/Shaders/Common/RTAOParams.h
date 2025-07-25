// © 2025 Adam Badke. All rights reserved.
#ifndef SE_RTAO_PARAMS
#define SE_RTAO_PARAMS

#include "PlatformConversions.h"

struct RTAOParamsData
{
	float4 g_params; // .x = TMin, .y = TMax, .z = ray count, .w = isEnabled
	uint4 g_indexes; // .x = depth texture idx, .y = wNormal tex idx, .zw = unused

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "RTAOParams";
#endif
};


#endif //SE_RTAO_PARAMS