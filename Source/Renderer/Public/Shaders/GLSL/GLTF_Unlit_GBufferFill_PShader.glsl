// © 2025 Adam Badke. All rights reserved.
#define VOUT_COLOR
#define SABER_INSTANCING
#include "SaberCommon.glsli"

#define SE_WRITE_GBUFFER
#include "GBufferCommon.glsli"

#include "UVUtils.glsli"

#include "../Common/InstancingParams.h"
#include "../Common/MaterialParams.h"


layout(binding=0) uniform sampler2D BaseColorTex;

layout(std430, binding = 0) readonly buffer InstanceIndexParams { InstanceIndexData _InstanceIndexParams[]; };
layout(std430, binding = 2) readonly buffer UnlitParams { UnlitData _UnlitParams[]; };


void PShader()
{
	const uint materialIdx = _InstanceIndexParams[InstanceParamsIn.InstanceID].g_indexes.y;

	const vec2 albedoUV = GetUV(In, _UnlitParams[materialIdx].g_uvChannelIndexes0.x);

	const vec4 matAlbedo = texture(BaseColorTex, albedoUV);

	// Alpha clipping
#if defined(DO_ALPHA_CLIP)
	const float alphaCutoff = _UnlitParams[materialIdx].g_alphaCutuff.x;
	if (matAlbedo.a < alphaCutoff)
	{
		discard;
	}
#endif

	// Albedo. Note: We use an sRGB-format texture, which converts this value from sRGB->linear space for free
	// g_baseColorFactor and In.Color are factored into the albedo as per the GLTF 2.0 specifications
	const vec4 baseColorFactor = _UnlitParams[materialIdx].g_baseColorFactor;
	Albedo = matAlbedo * baseColorFactor * In.Color;

	WorldNormal = vec4(0.f, 0.f, 0.f, 0.f);
	
	// RMAOVn:
	const float roughnessFactor = 1.f;	// GLTF Unlit default: Roughness factor > 0.5f
	const float metallicFactor = 0.f;	// GLTF Unlit default: Metallic factor = 0.f
	const float occlusion = 1.f;		// GLTF Unlit default: Occlusion is omitted

	RMAOVn = vec4(roughnessFactor, metallicFactor, occlusion, 0.f);

	// Emissive:
	Emissive = vec4(0.f, 0.f, 0.f, 0.f);

	// Material properties:
	MatProp0Vn = vec4(0.f, 0.f, 0.f, 0.f);

	// Material ID:
	const uint materialID = _UnlitParams[materialIdx].g_uvChannelIndexes0.y;
	MaterialID = materialID;
}