// © 2024 Adam Badke. All rights reserved.
#ifndef SE_LIGHT_PARAMS
#define SE_LIGHT_PARAMS

#include "PlatformConversions.h"


struct LightData
{
	float4 g_lightColorIntensity; // .rgb = hue, .a = intensity

	// .xyz = Point/spot lights: world pos. Directional lights: Normalized point -> source dir
	// .w = emitter radius (point/spot lights)
	float4 g_lightWorldPosRadius;
	float4 g_globalForwardDir; // .xyz = Local -Z (i.e. Direction light leaves the light source). .w = unused

	float4 g_intensityScale; // .xy = diffuse/specular intensity scale, .zw = spot light inner/outer angle

	float4x4 g_shadowCam_VP;

	float4 g_shadowMapTexelSize;	// .xyzw = width, height, 1/width, 1/height
	float4 g_shadowCamNearFarBiasMinMax; // .xy = shadow cam near/far, .zw = min, max shadow bias
	float4 g_shadowParams; // .x = has shadow (1.f), .y = quality mode, .zw = light size UV radius
	float4 g_renderTargetResolution; // .xy = xRes, yRes, .zw = 1/xRes 1/yRes

	// Type-specific extra values:
	// Point, directional: Unused
	// Spot: .xyz = pre-computed attenuation values: .x = cos(outerAngle), .y = scaleTerm, .z = offsetTerm
	float4 g_extraParams; 

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "LightParams";
#endif
};


struct AmbientLightData
{
	// .x = max PMREM mip level, .y = pre-integrated DFG texture width/height, .z diffuse scale, .w = specular scale
	float4 g_maxPMREMMipDFGResScaleDiffuseScaleSpec;
	float4 g_ssaoTexDims; // .xyzw = width, height, 1/width, 1/height

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "AmbientLightParams";
#endif
};


struct PoissonSampleParamsData
{
	float4 g_poissonSamples64[32]; // 64x float2
	float4 g_poissonSamples32[16]; // 32x float2
	float4 g_poissonSamples25[13]; // 25x float2

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "PoissonSampleParams";
#endif
};


#endif // SE_LIGHT_PARAMS