// © 2024 Adam Badke. All rights reserved.
#ifndef SE_DEBUG_PARAMS
#define SE_DEBUG_PARAMS

#include "Private/PlatformConversions.h"


struct DebugData
{
	float4 g_scales; // Line scales: .x = normal, .y = axis, .zw = unused

	// XYZ Axis (red/green/blue): [0], [1], [2]
	// Normals: [3]
	// Wireframe: [4]
	float4 g_colors[5];

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "DebugParams";
#endif
};


#endif // SE_DEBUG_PARAMS