// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_TBN
#define VOUT_COLOR
#define SABER_INSTANCING

#include "SaberCommon.hlsli"

#include "NormalMapUtils.hlsli"
#include "UVUtils.hlsli"

#include "../Common/InstancingParams.h"
#include "../Common/MaterialParams.h"


Texture2D<float4> BaseColorTex;
Texture2D<float4> NormalTex;
Texture2D<float4> MetallicRoughnessTex;
Texture2D<float4> OcclusionTex;
Texture2D<float4> EmissiveTex;

// Note: If a resource is used in multiple shader stages, we need to explicitely specify the register and space.
// Otherwise, shader reflection will assign the resource different registers for each stage (while SE expects them to be
// consistent). We (currently) use space1 for all explicit bindings, preventing conflicts with non-explicit bindings in
// space0
StructuredBuffer<InstanceIndexData> InstanceIndexParams : register(t0, space1);
StructuredBuffer<PBRMetallicRoughnessData> InstancedPBRMetallicRoughnessParams : register(t2, space1);


struct GBufferOut
{
	float4 Albedo		: SV_Target0;
	float4 WorldNormal	: SV_Target1;
	float4 RMAOVn		: SV_Target2;	
	float4 Emissive		: SV_Target3;
	float4 MatProp0Vn	: SV_Target4;
};


GBufferOut PShader(VertexOut In)
{
	GBufferOut output;
	
	const uint materialIdx = InstanceIndexParams[In.InstanceID].g_materialIdx;
	
	const float2 albedoUV = GetUV(In, 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes0.x);
	
	const float2 metallicRoughnessUV = GetUV(In,
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes0.y);
	
	const float2 normalUV = GetUV(In,
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes0.z);
	
	const float2 occlusionUV = GetUV(In,
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes0.w);
	
	const float2 emissiveUV = GetUV(In,
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes1.x);
	
	const float4 matAlbedo = BaseColorTex.Sample(WrapAnisotropic, albedoUV);
	
	// Alpha clipping
#if defined(DO_ALPHA_CLIP)
	const float alphaCutoff = InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_f0AlphaCutoff.w;
	clip(matAlbedo.a < alphaCutoff ? -1 : 1); 
#endif
	
	const float4 baseColorFactor = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_baseColorFactor;
	output.Albedo = matAlbedo * baseColorFactor * In.Color;

	// Vertex normal:
	const float3 vertexNormal = float3(In.TBN[0].z, In.TBN[1].z, In.TBN[2].z);
	const float2 encodedVertexNormal = EncodeOctohedralNormal(vertexNormal);
	
	// World-space normal:
	// Note: We normalize the normal after applying the TBN and writing to the GBuffer, we may need to post-apply this
	const float normalScaleFactor = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.z;
	const float3 normalScale = float3(normalScaleFactor, normalScaleFactor, 1.f);
	const float3 texNormal = NormalTex.Sample(WrapAnisotropic, normalUV).xyz;
	output.WorldNormal = float4(WorldNormalFromTextureNormal(texNormal, normalScale, In.TBN), 0.f);
	
	// RMAOVn:
	const float roughnessFactor = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.y;
	
	const float metallicFactor = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.x;
	
	// Unpack/scale metallic/roughness: .G = roughness, .B = metalness
	const float2 roughnessMetalness = 
		MetallicRoughnessTex.Sample(WrapAnisotropic, metallicRoughnessUV).gb * float2(roughnessFactor, metallicFactor);
	
	const float occlusionStrength = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.w;
	const float occlusion = OcclusionTex.Sample(WrapAnisotropic, occlusionUV).r * occlusionStrength;
	
	output.RMAOVn = float4(roughnessMetalness, occlusion, encodedVertexNormal.x);
	
	// Emissive:
	const float3 emissiveFactor = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_emissiveFactorStrength.rgb;
	const float emissiveStrength = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_emissiveFactorStrength.w;
	
	const float3 emissive = 
		EmissiveTex.Sample(WrapAnisotropic, emissiveUV).rgb * emissiveFactor * emissiveStrength;
	
	output.Emissive = float4(emissive, 1.f);
	
	output.MatProp0Vn = float4(
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_f0AlphaCutoff.xyz,
		encodedVertexNormal.y);
	
	return output;
}