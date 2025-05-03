// © 2024 Adam Badke. All rights reserved.
#ifndef SE_SHADOW_RENDER_PARAMS
#define SE_SHADOW_RENDER_PARAMS

#include "PlatformConversions.h"


struct CubemapShadowData
{
	float4x4 g_cubemapShadowCam_VP[6];
	float4 g_cubemapShadowCamNearFar; // .xy = near, far. .zw = unused
	float4 g_cubemapLightWorldPos; // .xyz = light word pos, .w = unused

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "CubemapShadowParams";
#endif
};


#endif // SE_SHADOW_RENDER_PARAMS