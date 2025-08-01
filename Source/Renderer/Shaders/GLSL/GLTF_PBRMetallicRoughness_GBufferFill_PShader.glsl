// © 2023 Adam Badke. All rights reserved.
#define VOUT_COLOR
#define VOUT_TBN
#define SABER_INSTANCING
#include "SaberCommon.glsli"

#define SE_WRITE_GBUFFER
#include "GBufferCommon.glsli"

#include "NormalMapUtils.glsli"
#include "UVUtils.glsli"

#include "../Common/InstancingParams.h"
#include "../Common/MaterialParams.h"


layout(std430, binding = 0) readonly buffer InstanceIndexParams { InstanceIndexData _InstanceIndexParams[]; };

layout(std430, binding = 2) readonly buffer PBRMetallicRoughnessParams {	PBRMetallicRoughnessData _PBRMetallicRoughnessParams[]; };

layout(binding = 0) uniform sampler2D BaseColorTex;
layout(binding = 1) uniform sampler2D MetallicRoughnessTex;
layout(binding = 2) uniform sampler2D NormalTex;
layout(binding = 3) uniform sampler2D OcclusionTex;
layout(binding = 4) uniform sampler2D EmissiveTex;


void PShader()
{
	const uint materialIdx = _InstanceIndexParams[InstanceParamsIn.InstanceID].g_indexes.y;

	const vec2 albedoUV = GetUV(In, 
		_PBRMetallicRoughnessParams[materialIdx].g_uvChannelIndexes0.x);
	
	const vec2 metallicRoughnessUV = GetUV(In,
		_PBRMetallicRoughnessParams[materialIdx].g_uvChannelIndexes0.y);
	
	const vec2 normalUV = GetUV(In,
		_PBRMetallicRoughnessParams[materialIdx].g_uvChannelIndexes0.z);
	
	const vec2 occlusionUV = GetUV(In,
		_PBRMetallicRoughnessParams[materialIdx].g_uvChannelIndexes0.w);
	
	const vec2 emissiveUV = GetUV(In,
		_PBRMetallicRoughnessParams[materialIdx].g_uvChannelIndexes1.x);

	const vec4 matAlbedo = texture(BaseColorTex, albedoUV);

	// Alpha clipping
#if defined(DO_ALPHA_CLIP)
	const float alphaCutoff = _PBRMetallicRoughnessParams[materialIdx].g_f0AlphaCutoff.w;
	if (matAlbedo.a < alphaCutoff)
	{
		discard;
	}
#endif

	// Albedo. Note: We use an sRGB-format texture, which converts this value from sRGB->linear space for free
	// Factor in g_baseColorFactor In.Color to the albedo as per the GLTF 2.0 specifications
	const vec4 baseColorFactor = _PBRMetallicRoughnessParams[materialIdx].g_baseColorFactor;
	Albedo = matAlbedo * In.Color * baseColorFactor;

	// Vertex normal:
	const vec3 vertexNormal = In.TBN[2]; // worldFaceNormal
	const vec2 encodedVertexNormal = EncodeOctohedralNormal(vertexNormal);

	// World-space normal:
	const float normalScaleFactor = _PBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.z;
	const vec3 normalScale = vec3(normalScaleFactor, normalScaleFactor, 1.f); // Scales the normal's X, Y directions
	const vec3 texNormal = texture(NormalTex, normalUV).xyz;
	const vec3 worldNormal = WorldNormalFromTextureNormal(texNormal, In.TBN) * normalScale;

	WorldNormal = vec4(worldNormal, 0.0f);
	
	// Unpack/scale metallic/roughness: .G = roughness, .B = metallness
	const float metallicFactor = _PBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.x;
	const float roughnessFactor = _PBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.y;
	const vec2 roughMetal = texture(MetallicRoughnessTex, metallicRoughnessUV).gb * vec2(roughnessFactor, metallicFactor);

	// Unpack/scale AO:
	const float occlusionStrength = _PBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.w;
	const float occlusion = texture(OcclusionTex, occlusionUV).r * occlusionStrength;
	//const float occlusion = clamp((1.0f + g_occlusionStrength) * (texture(OcclusionTex, occlusionUV).r - 1.0f), 0.0f, 1.0f);
	// TODO: GLTF specifies the above occlusion scaling, but CGLTF seems non-compliant & packs occlusion strength into
	// the texture scale value. For now, just use something sane.
	
	// Pack RMAO: 
	RMAOVn = vec4(roughMetal, occlusion, encodedVertexNormal.x);

	// Emissive:
	const vec3 emissiveFactor = _PBRMetallicRoughnessParams[materialIdx].g_emissiveFactorStrength.rgb;
	const float emissiveStrength = _PBRMetallicRoughnessParams[materialIdx].g_emissiveFactorStrength.w;
	vec3 emissive = texture(EmissiveTex, emissiveUV).rgb * emissiveFactor * emissiveStrength;

	Emissive = vec4(emissive, 1.0f);

	// Material properties:
	MatProp0Vn = vec4(
		_PBRMetallicRoughnessParams[materialIdx].g_f0AlphaCutoff.xyz,
		encodedVertexNormal.y);

	// Material ID:
	const uint materialID = _PBRMetallicRoughnessParams[materialIdx].g_uvChannelIndexes1.y;
	MaterialID = materialID;
}