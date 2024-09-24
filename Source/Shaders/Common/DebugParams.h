// © 2024 Adam Badke. All rights reserved.
#ifndef SE_DEBUG_PARAMS
#define SE_DEBUG_PARAMS

#include "PlatformConversions.h"


struct DebugData
{
	float4 g_normalColor;

	float4 g_axisScales; // Axis line scales: .xyzw = world CS, mesh CS, LightCS, Camera CS

	float4 g_scales; // Line scales: .x = normal, .yzw = unused
	

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "DebugParams";
#endif
};


#endif // SE_DEBUG_PARAMS