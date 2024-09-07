// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_COLOR
#define VOUT_TBN
#define SABER_INSTANCING

#include "AmbientCommon.hlsli"
#include "Lighting.hlsli"
#include "NormalMapUtils.hlsli"
#include "SaberCommon.hlsli"
#include "Shadows.hlsli"

#include "../Common/LightParams.h"


uint UnpackPointLightIndex(uint arrayIdx)
{
	return ((uint[MAX_LIGHT_COUNT])AllLightIndexesParams.g_pointIndexes)[arrayIdx];
}


uint UnpackSpotLightIndex(uint arrayIdx)
{
	return ((uint[MAX_LIGHT_COUNT])AllLightIndexesParams.g_spotIndexes)[arrayIdx];
}


float4 PShader(VertexOut In) : SV_Target
{
	const uint materialIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_materialIdx;
	
	const float3 worldPos = In.WorldPos;
	
	const float4 matAlbedo = MatAlbedo.Sample(WrapAnisotropic, In.UV0);
	const float4 baseColorFactor =
			InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_baseColorFactor;
	const float3 linearAlbedo = (matAlbedo * baseColorFactor * In.Color).rgb;
	
	const float normalScaleFactor =
		InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.z;
	const float3 normalScale = float3(normalScaleFactor, normalScaleFactor, 1.f);
	const float3 texNormal = MatNormal.Sample(WrapAnisotropic, In.UV0).xyz;
	const float3 worldNormal = WorldNormalFromTextureNormal(texNormal, normalScale, In.TBN);
	
	const float linearRoughnessFactor =
			InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.y;
	
	const float metallicFactor =
			InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.x;
	
	const float2 roughnessMetalness =
			MatMetallicRoughness.Sample(WrapAnisotropic, In.UV0).gb * float2(linearRoughnessFactor, metallicFactor);
	
	const float remappedRoughness = RemapRoughness(roughnessMetalness.x);
	
	const float3 f0 = InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_f0.xyz;
	
	float3 totalContribution = 0.f;
	
	// Ambient:
	{
		AmbientLightingParams ambientLightParams;
	
		ambientLightParams.WorldPosition = worldPos;
		ambientLightParams.V = normalize(CameraParams.g_cameraWPos.xyz - worldPos); // point -> camera
		ambientLightParams.WorldNormal = worldNormal;
		
		ambientLightParams.LinearAlbedo = linearAlbedo;
		ambientLightParams.DielectricSpecular = f0;
	
		ambientLightParams.LinearMetalness = roughnessMetalness.y;
		ambientLightParams.LinearRoughness = roughnessMetalness.x;
		ambientLightParams.RemappedRoughness = remappedRoughness;

		const float occlusionStrength =
			InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.w;
		const float occlusion = MatOcclusion.Sample(WrapAnisotropic, In.UV0).r * occlusionStrength;
	
		ambientLightParams.FineAO = occlusion;
		ambientLightParams.CoarseAO = 1.f; // No SSAO for transparents
	
		totalContribution += ComputeAmbientLighting(ambientLightParams);
	}

	// Directional:
	{
		const uint numDirectionalLights = AllLightIndexesParams.g_numLights.x;
		
		for (uint directionalIdx = 0; directionalIdx < numDirectionalLights; ++directionalIdx)
		{
			const LightData lightData = DirectionalLightParams[directionalIdx];
			
			const uint shadowIdx = lightData.g_extraParams.w;
			
			const float2 shadowCamNearFar = lightData.g_shadowCamNearFarBiasMinMax.xy;
			const float2 minMaxShadowBias = lightData.g_shadowCamNearFarBiasMinMax.zw;
			const float2 lightUVRadiusSize = lightData.g_shadowParams.zw;
			const float shadowQualityMode = lightData.g_shadowParams.y;
		
			const bool shadowEnabled = lightData.g_shadowParams.x > 0.f;
			const float shadowFactor = shadowEnabled ?
			Get2DShadowFactor(
				worldPos,
				worldNormal,
				lightData.g_lightWorldPosRadius.xyz,
				lightData.g_shadowCam_VP,
				shadowCamNearFar,
				minMaxShadowBias,
				shadowQualityMode,
				lightUVRadiusSize,
				lightData.g_shadowMapTexelSize,
				DirectionalShadows,
				shadowIdx) : 1.f;
		
			LightingParams lightingParams;
			lightingParams.LinearAlbedo = linearAlbedo;
			lightingParams.WorldNormal = worldNormal;
			lightingParams.LinearRoughness = roughnessMetalness.x;
			lightingParams.RemappedRoughness = remappedRoughness;
			lightingParams.LinearMetalness = roughnessMetalness.y;
			lightingParams.WorldPosition = worldPos;
			lightingParams.F0 = f0;
	
			const float NoL = saturate(dot(worldNormal, lightData.g_lightWorldPosRadius.xyz));
			lightingParams.NoL = NoL;
	
			lightingParams.LightWorldPos = worldPos; // Ensure attenuation = 0
			lightingParams.LightWorldDir = lightData.g_lightWorldPosRadius.xyz;
			lightingParams.LightColor = lightData.g_lightColorIntensity.rgb;
			lightingParams.LightIntensity = lightData.g_lightColorIntensity.a;
			lightingParams.LightAttenuationFactor = 1.f;
	
			lightingParams.ShadowFactor = shadowFactor;
	
			lightingParams.CameraWorldPos = CameraParams.g_cameraWPos.xyz;
			lightingParams.Exposure = CameraParams.g_exposureProperties.x;
	
			lightingParams.DiffuseScale = lightData.g_intensityScale.x;
			lightingParams.SpecularScale = lightData.g_intensityScale.y;
		
			totalContribution += ComputeLighting(lightingParams);
		}
	}
	
	// Point:
	{
		const uint numPointLights = AllLightIndexesParams.g_numLights.y;
		
		for (uint pointIdx = 0; pointIdx < numPointLights; ++pointIdx)
		{
			const uint pointLightDataIdx = UnpackPointLightIndex(pointIdx);

			const LightData lightData = PointLightParams[pointLightDataIdx];
			
			const uint shadowIdx = lightData.g_extraParams.w;
			
			const float3 lightWorldPos = lightData.g_lightWorldPosRadius.xyz;
			const float3 lightWorldDir = normalize(lightWorldPos - worldPos);
			
			// Convert luminous power (phi) to luminous intensity (I):
			const float luminousIntensity = lightData.g_lightColorIntensity.a * M_1_4PI;
			
			const float emitterRadius = lightData.g_lightWorldPosRadius.w;
			const float attenuationFactor = ComputeNonSingularAttenuationFactor(worldPos, lightWorldPos, emitterRadius);
			
			const float2 shadowCamNearFar = lightData.g_shadowCamNearFarBiasMinMax.xy;
			const float2 minMaxShadowBias = lightData.g_shadowCamNearFarBiasMinMax.zw;
			const float cubeFaceDimension = lightData.g_shadowMapTexelSize.x; // Assume the cubemap width/height are the same
			const float2 lightUVRadiusSize = lightData.g_shadowParams.zw;
			const float shadowQualityMode = lightData.g_shadowParams.y;
	
			const bool shadowEnabled = lightData.g_shadowParams.x > 0.f;
			const float shadowFactor = shadowEnabled ?
			GetCubeShadowFactor(
				worldPos,
				worldNormal,
				lightWorldPos,
				lightWorldDir,
				shadowCamNearFar,
				minMaxShadowBias,
				shadowQualityMode,
				lightUVRadiusSize,
				cubeFaceDimension,
				PointShadows,
				shadowIdx) : 1.f;
			
			LightingParams lightingParams;
			lightingParams.LinearAlbedo = linearAlbedo;
			lightingParams.WorldNormal = worldNormal;
			lightingParams.LinearRoughness = roughnessMetalness.x;
			lightingParams.RemappedRoughness = remappedRoughness;
			lightingParams.LinearMetalness = roughnessMetalness.y;
			lightingParams.WorldPosition = worldPos;
			lightingParams.F0 = f0;
	
			const float NoL = saturate(dot(worldNormal, lightWorldDir));
			lightingParams.NoL = NoL;
	
			lightingParams.LightWorldPos = lightWorldPos;
			lightingParams.LightWorldDir = lightWorldDir;
			lightingParams.LightColor = lightData.g_lightColorIntensity.rgb;
			lightingParams.LightIntensity = luminousIntensity;
			lightingParams.LightAttenuationFactor = attenuationFactor;
	
			lightingParams.ShadowFactor = shadowFactor;
	
			lightingParams.CameraWorldPos = CameraParams.g_cameraWPos.xyz;
			lightingParams.Exposure = CameraParams.g_exposureProperties.x;
	
			lightingParams.DiffuseScale = lightData.g_intensityScale.x;
			lightingParams.SpecularScale = lightData.g_intensityScale.y;
			
			totalContribution += ComputeLighting(lightingParams);
		}
	}
	
	// Spot:
	{
		const uint numSpotLights = AllLightIndexesParams.g_numLights.z;
		
		for (uint spotIdx = 0; spotIdx < numSpotLights; ++spotIdx)
		{		
			const uint spotLightDataIdx = UnpackSpotLightIndex(spotIdx);
	
			const LightData lightData = SpotLightParams[spotLightDataIdx];
			
			const uint shadowIdx = lightData.g_extraParams.w;
			
			const float3 lightWorldPos = lightData.g_lightWorldPosRadius.xyz;
			const float3 lightWorldDir = normalize(lightWorldPos - worldPos);
	
			// Convert luminous power (phi) to luminous intensity (I):
			const float luminousIntensity = lightData.g_lightColorIntensity.a * M_1_PI;
	
			const float emitterRadius = lightData.g_lightWorldPosRadius.w;
			const float distanceAttenuation = ComputeNonSingularAttenuationFactor(worldPos, lightWorldPos, emitterRadius);
	
			const float angleAttenuation = GetSpotlightAngleAttenuation(
				lightWorldDir,
				lightData.g_globalForwardDir.xyz,
				lightData.g_intensityScale.z,
				lightData.g_intensityScale.w,
				lightData.g_extraParams.x,
				lightData.g_extraParams.y,
				lightData.g_extraParams.z);
	
			const float combinedAttenuation = distanceAttenuation * angleAttenuation;
	
			const float2 shadowCamNearFar = lightData.g_shadowCamNearFarBiasMinMax.xy;
			const float2 minMaxShadowBias = lightData.g_shadowCamNearFarBiasMinMax.zw;
			const float2 lightUVRadiusSize = lightData.g_shadowParams.zw;
			const float shadowQualityMode = lightData.g_shadowParams.y;
	
			const bool shadowEnabled = lightData.g_shadowParams.x > 0.f;
			const float shadowFactor = shadowEnabled ?
				Get2DShadowFactor(
					worldPos,
					worldNormal,
					lightWorldPos,
					lightData.g_shadowCam_VP,
					shadowCamNearFar,
					minMaxShadowBias,
					shadowQualityMode,
					lightUVRadiusSize,
					lightData.g_shadowMapTexelSize,
					SpotShadows,
					shadowIdx) : 1.f;
	
			LightingParams lightingParams;
			lightingParams.LinearAlbedo = linearAlbedo;
			lightingParams.WorldNormal = worldNormal;
			lightingParams.LinearRoughness = roughnessMetalness.x;
			lightingParams.RemappedRoughness = remappedRoughness;
			lightingParams.LinearMetalness = roughnessMetalness.y;
			lightingParams.WorldPosition = worldPos;
			lightingParams.F0 = f0;
			
			const float NoL = saturate(dot(worldNormal, lightWorldDir));
			lightingParams.NoL = NoL;
	
			lightingParams.LightWorldPos = lightWorldPos;
			lightingParams.LightWorldDir = lightWorldDir;
			lightingParams.LightColor = lightData.g_lightColorIntensity.rgb;
			lightingParams.LightIntensity = luminousIntensity;
			lightingParams.LightAttenuationFactor = combinedAttenuation;
	
			lightingParams.ShadowFactor = shadowFactor;
	
			lightingParams.CameraWorldPos = CameraParams.g_cameraWPos.xyz;
			lightingParams.Exposure = CameraParams.g_exposureProperties.x;
	
			lightingParams.DiffuseScale = lightData.g_intensityScale.x;
			lightingParams.SpecularScale = lightData.g_intensityScale.y;
	
			totalContribution.rgb += ComputeLighting(lightingParams);
		}
	}
	
	return float4(totalContribution, matAlbedo.a);
}