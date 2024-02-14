// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_TBN
#define VOUT_COLOR
#define VOUT_INSTANCE_ID

#include "NormalMapUtils.hlsli"
#include "SaberCommon.hlsli"


struct GBufferOut
{
	float4 Albedo				: SV_Target0;
	float4 WorldNormal			: SV_Target1;
	float4 RMAO					: SV_Target2;	
	float4 Emissive				: SV_Target3;
	float4 MatProp0				: SV_Target4;
};

GBufferOut PShader(VertexOut In)
{
	GBufferOut output;
	
	const float4 matAlbedo = MatAlbedo.Sample(WrapAnisotropic, In.UV0);
	clip(matAlbedo.a < ALPHA_CUTOFF ? -1 : 1); // Alpha clipping
	
	const uint materialIdx = InstanceIndexParams[In.InstanceID].g_materialIdx;
	
	const float4 baseColorFactor = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_baseColorFactor;
	output.Albedo = matAlbedo * baseColorFactor * In.Color;

	// World-space normal:
	// Note: We normalize the normal after applying the TBN and writing to the GBuffer, we may need to post-apply this
	const float normalScaleFactor = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_normalScale;
	const float3 normalScale = float3(normalScaleFactor, normalScaleFactor, 1.f);
	const float3 texNormal = MatNormal.Sample(WrapAnisotropic, In.UV0).xyz;
	output.WorldNormal = float4(WorldNormalFromTextureNormal(texNormal, normalScale, In.TBN), 0.f);
	
	// RMAO:
	const float roughnessFactor = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_roughnessFactor;
	
	const float metallicFactor = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metallicFactor;
	
	// Unpack/scale metallic/roughness: .G = roughness, .B = metallness
	const float2 roughnessMetalness = 
		MatMetallicRoughness.Sample(WrapAnisotropic, In.UV0).gb * float2(roughnessFactor, metallicFactor);
	
	const float occlusionStrength = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_occlusionStrength;
	const float occlusion = MatOcclusion.Sample(WrapAnisotropic, In.UV0).r * occlusionStrength;
	
	output.RMAO = float4(roughnessMetalness, occlusion, 1.f);
	
	// Emissive:
	const float3 emissiveFactor = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_emissiveFactorStrength.rgb;
	const float emissiveStrength = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_emissiveFactorStrength.w;
	
	const float3 emissive = 
		MatEmissive.Sample(WrapAnisotropic, In.UV0).rgb * emissiveFactor * emissiveStrength;
	
	output.Emissive = float4(emissive, 1.f);
	
	output.MatProp0 = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_f0; // .xyz = f0, .w = unused
	
	return output;
}