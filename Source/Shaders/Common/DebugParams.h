// © 2024 Adam Badke. All rights reserved.
#ifndef SE_DEBUG_PARAMS
#define SE_DEBUG_PARAMS

#include "PlatformConversions.h"


struct DebugData
{
	float4 g_axisScales; // Axis scales: .x = world CS, .y = mesh CS, .z = LightCS, .w = Camera CS

	float4 g_scales; // Line scales: .x = normal, .yzw = unused

	// XYZ Axis (red/green/blue): [0], [1], [2]
	// Normals: [3]
	float4 g_colors[4];

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "DebugParams";
#endif
};


#endif // SE_DEBUG_PARAMS