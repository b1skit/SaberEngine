// © 2024 Adam Badke. All rights reserved.
#ifndef SE_SHADOW_RENDER_PARAMS
#define SE_SHADOW_RENDER_PARAMS

#include "PlatformConversions.h"


struct CubeShadowRenderData
{
	float4x4 g_cubemapShadowCam_VP[6];
	float4 g_cubemapShadowCamNearFar; // .xy = near, far. .zw = unused
	float4 g_cubemapLightWorldPos; // .xyz = light word pos, .w = unused


#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "CubeShadowRenderParams";
#endif
};


struct ShadowData
{
	float4x4 g_shadowCam_VP;

	float4 g_shadowMapTexelSize;			// .xyzw = width, height, 1/width, 1/height
	float4 g_shadowCamNearFarBiasMinMax;	// .xy = shadow cam near/far, .zw = min, max shadow bias
	float4 g_shadowParams;					// .x = shadow enabled?, .y = quality mode, .zw = light size UV radius


#if defined(__cplusplus)
	static constexpr char const* s_shaderName = "ShadowParams";
#endif
};


#endif // SE_SHADOW_RENDER_PARAMS