// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_COLOR
#define VOUT_WORLD_POS
#define VOUT_TBN
#define VOUT_INSTANCE_ID

#include "AmbientCommon.hlsli"
#include "NormalMapUtils.hlsli"
#include "SaberCommon.hlsli"


float4 PShader(VertexOut In) : SV_Target
{
	const uint materialIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_materialIdx;
	
	AmbientLightingParams lightingParams;
	
	const float3 worldPos = In.WorldPos;
	lightingParams.WorldPosition = worldPos;
	
	lightingParams.V = normalize(CameraParams.g_cameraWPos.xyz - worldPos); // point -> camera
	
	const float normalScaleFactor =
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.z;
	const float3 normalScale = float3(normalScaleFactor, normalScaleFactor, 1.f);
	const float3 texNormal = MatNormal.Sample(WrapAnisotropic, In.UV0).xyz;
	const float3 worldNormal = WorldNormalFromTextureNormal(texNormal, normalScale, In.TBN);
	lightingParams.WorldNormal = worldNormal;
	
	const float4 matAlbedo = MatAlbedo.Sample(WrapAnisotropic, In.UV0);
	const float4 baseColorFactor =
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_baseColorFactor;
	lightingParams.LinearAlbedo = (matAlbedo * baseColorFactor * In.Color).rgb;
	
	lightingParams.DielectricSpecular = 
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_f0.rgb;
	
	const float linearRoughnessFactor =
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.y;
	
	const float metallicFactor =
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.x;
	
	const float2 roughnessMetalness =
		MatMetallicRoughness.Sample(WrapAnisotropic, In.UV0).gb * float2(linearRoughnessFactor, metallicFactor);
	
	lightingParams.LinearMetalness = roughnessMetalness.y;
	
	lightingParams.LinearRoughness = roughnessMetalness.x;
	lightingParams.RemappedRoughness = RemapRoughness(roughnessMetalness.x);

	const float occlusionStrength =
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.w;
	const float occlusion = MatOcclusion.Sample(WrapAnisotropic, In.UV0).r * occlusionStrength;
	
	lightingParams.FineAO = occlusion;
	lightingParams.CoarseAO = 1.f; // No SSAO for transparents
	
	return float4(ComputeAmbientLighting(lightingParams), matAlbedo.a);
}