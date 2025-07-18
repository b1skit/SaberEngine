// © 2024 Adam Badke. All rights reserved.
#ifndef SE_LIGHT_PARAMS
#define SE_LIGHT_PARAMS

#include "PlatformConversions.h"

#define INVALID_SHADOW_IDX 0xFFFFFFFF

// As per the fr/gr::Light::Type
#define LIGHT_TYPE_DIRECTIONAL 1
#define LIGHT_TYPE_POINT 2
#define LIGHT_TYPE_SPOT 3

struct AmbientLightData
{
	// .x = max PMREM mip level, .y = pre-integrated DFG texture width/height, .z diffuse scale, .w = specular scale
	float4 g_maxPMREMMipDFGResScaleDiffuseScaleSpec;
	float4 g_AOTexDims; // .xyzw = width, height, 1/width, 1/height

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "AmbientLightParams";
#endif
};


struct LightData
{
	float4 g_lightColorIntensity; // .rgb = hue, .a = intensity

	// .xyz = Point/spot lights: world pos. Directional lights: Normalized point -> source dir
	// .w = emitter radius (point/spot lights)
	float4 g_lightWorldPosRadius;
	float4 g_globalForwardDir; // .xyz = Local -Z (i.e. Direction light leaves the light source). .w = unused

	float4 g_intensityScale; // .xy = diffuse/specular intensity scale, .zw = spot light inner/outer angle

	// Type-specific extra values:
	// - Directional/Point/: .xyzw = unused
	// - Spot: .xyz = attenuation values (.x = cos(outerAngle), .y = scaleTerm, .z = offsetTerm), .w = unused
	float4 g_extraParams;	


#if defined(__cplusplus)
	static constexpr char const* s_directionalLightDataShaderName = "DirectionalLightParams";
	static constexpr char const* s_pointLightDataShaderName = "PointLightParams";
	static constexpr char const* s_spotLightDataShaderName = "SpotLightParams";
#endif
};


struct LightShadowLUTData
{
	// .x = light buffer idx, .y = shadow buffer idx (INVALID_SHADOW_IDX == no shadow), .z = shadow tex array idx, .w = light type
	uint4 g_lightShadowIdx; 


#if defined(__cplusplus)
	static constexpr char const* const s_shaderNameDirectional = "DirectionalLUT";
	static constexpr char const* const s_shaderNamePoint = "PointLUT";
	static constexpr char const* const s_shaderNameSpot = "SpotLUT";


	inline static void SetLightBufferIndex(uint32_t lutIdx, void* dst)
	{
		static_cast<LightShadowLUTData*>(dst)->g_lightShadowIdx.x = lutIdx;
	}

	inline static void SetShadowBufferIndex(uint32_t lutIdx, void* dst)
	{
		static_cast<LightShadowLUTData*>(dst)->g_lightShadowIdx.y = lutIdx;
	}
#endif
};


struct LightMetadata
{
	uint4 g_numLights; // .x = No. directional, .y = No. point lights, .z = No. spot lights, .w = unused

#if defined(__cplusplus)
	static constexpr char const* s_shaderName = "LightCounts";
#endif
};



#endif // SE_LIGHT_PARAMS