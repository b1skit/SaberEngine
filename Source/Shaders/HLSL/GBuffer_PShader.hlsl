// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN
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
	
	const float4 matAlbedo = MatAlbedo.Sample(WrapLinearLinear, In.UV0);
	clip(matAlbedo.a < ALPHA_CUTOFF ? -1 : 1); // Alpha clipping
	
	output.Albedo = matAlbedo * PBRMetallicRoughnessParams.g_baseColorFactor * In.Color;

	// World-space normal:
	const float3 texNormal = MatNormal.Sample(WrapLinearLinear, In.UV0).xyz;
	output.WorldNormal = float4(WorldNormalFromTextureNormal(texNormal, In.TBN), 0.f);
	
	// RMAO:
	const float roughnessFactor = PBRMetallicRoughnessParams.g_roughnessFactor;
	const float metallicFactor = PBRMetallicRoughnessParams.g_metallicFactor;
	
	// Unpack/scale metallic/roughness: .G = roughness, .B = metallness
	const float2 roughnessMetalness = 
		MatMetallicRoughness.Sample(WrapLinearLinear, In.UV0).gb * float2(roughnessFactor, metallicFactor);
	
	const float occlusionStrength = PBRMetallicRoughnessParams.g_occlusionStrength;
	const float occlusion = MatOcclusion.Sample(WrapLinearLinear, In.UV0).r * occlusionStrength;
	
	output.RMAO = float4(roughnessMetalness, occlusion, 1.f);
	
	// Emissive:
	const float3 emissiveFactor = PBRMetallicRoughnessParams.g_emissiveFactorStrength.rgb;
	const float emissiveStrength = PBRMetallicRoughnessParams.g_emissiveFactorStrength.w;
	
	float3 emissive = MatEmissive.Sample(WrapLinearLinear, In.UV0).rgb * emissiveFactor * emissiveStrength;
	
	// Emissive is light: Apply exposure now:
	const float exposure = CameraParams.g_exposureProperties.x;
	const float ev100 = CameraParams.g_exposureProperties.y;
	const float emissiveExposureCompensation = CameraParams.g_exposureProperties.z;
	
	emissive *= pow(2.f, ev100 + emissiveExposureCompensation - 3.f);
	emissive *= exposure;
	
	output.Emissive = float4(emissive, 1.f);
	
	output.MatProp0 = PBRMetallicRoughnessParams.g_f0; // .xyz = f0, .w = unused
	
	return output;
}