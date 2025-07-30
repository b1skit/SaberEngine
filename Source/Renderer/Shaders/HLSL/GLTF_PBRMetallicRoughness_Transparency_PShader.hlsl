// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_COLOR
#define VOUT_TBN
#define SABER_INSTANCING
#include "SaberCommon.hlsli"

#include "AmbientCommon.hlsli"
#include "Lighting.hlsli"
#include "NormalMapUtils.hlsli"
#include "Shadows.hlsli"

#include "../Common/CameraParams.h"
#include "../Common/InstancingParams.h"
#include "../Common/LightParams.h"
#include "../Common/MaterialParams.h"

Texture2D<float4> BaseColorTex;
Texture2D<float4> NormalTex;
Texture2D<float4> MetallicRoughnessTex;
Texture2D<float4> OcclusionTex;

Texture2DArray<float> DirectionalShadows;
TextureCubeArray<float> PointShadows;
Texture2DArray<float> SpotShadows;

StructuredBuffer<LightData> DirectionalLightParams;
StructuredBuffer<LightData> PointLightParams;
StructuredBuffer<LightData> SpotLightParams;

StructuredBuffer<ShadowData> ShadowParams;

// We access all light types simultaneously, thus we need arrays of LightShadowLUTData for each
StructuredBuffer<LightShadowLUTData> DirectionalLUT;
StructuredBuffer<LightShadowLUTData> PointLUT;
StructuredBuffer<LightShadowLUTData> SpotLUT;

ConstantBuffer<CameraData> CameraParams : register(space1);
ConstantBuffer<LightMetadata> LightCounts;

// Note: If a resource is used in multiple shader stages, we need to explicitely specify the register and space.
// Otherwise, shader reflection will assign the resource different registers for each stage (while SE expects them to be
// consistent). We (currently) use space1 for all explicit bindings, preventing conflicts with non-explicit bindings in
// space0
StructuredBuffer<InstanceIndexData> InstanceIndexParams : register(t0, space1);
StructuredBuffer<PBRMetallicRoughnessData> PBRMetallicRoughnessParams : register(t2, space1);


#if defined(SHADOWS_RAYTRACED)
#include "RayTracingCommon.hlsli"
#include "../Common/RayTracingParams.h"

RaytracingAccelerationStructure SceneBVH;
ConstantBuffer<TraceRayInlineData> TraceRayInlineParams;
#endif


float4 PShader(VertexOut In) : SV_Target
{
	const uint materialIdx = InstanceIndexParams[In.InstanceID].g_indexes.y;
	
	const float2 albedoUV = GetUV(In,
		PBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes0.x);
	
	const float2 metallicRoughnessUV = GetUV(In,
		PBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes0.y);
	
	const float2 normalUV = GetUV(In,
		PBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes0.z);
	
	const float2 occlusionUV = GetUV(In,
		PBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes0.w);
	
	//const float2 emissiveUV = GetUV(In,
	//	PBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes1.x);
	
	const float3 worldPos = In.WorldPos;
	
	const float4 matAlbedo = BaseColorTex.Sample(WrapAnisotropic, albedoUV);
	const float4 baseColorFactor = PBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_baseColorFactor;
	const float3 linearAlbedo = (matAlbedo * In.Color * baseColorFactor).rgb;
	
	const float normalScaleFactor =
		PBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.z;
	const float3 normalScale = float3(normalScaleFactor, normalScaleFactor, 1.f);
	const float3 texNormal = NormalTex.Sample(WrapAnisotropic, normalUV).xyz;
	const float3 worldNormal = WorldNormalFromTextureNormal(texNormal, normalScale, In.TBN);
	
	const float linearRoughnessFactor =
			PBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.y;
	
	const float metallicFactor =
			PBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.x;
	
	const float2 roughnessMetalness =
			MetallicRoughnessTex.Sample(WrapAnisotropic, metallicRoughnessUV).gb * float2(linearRoughnessFactor, metallicFactor);
	
	const float remappedRoughness = RemapRoughness(roughnessMetalness.x);
	
	const float3 f0 = PBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_f0AlphaCutoff.xyz;
	
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
			PBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_metRoughNmlOccScales.w;
		const float occlusion = OcclusionTex.Sample(WrapAnisotropic, occlusionUV).r * occlusionStrength;
	
		ambientLightParams.FineAO = occlusion;
		ambientLightParams.CoarseAO = 1.f; // No SSAO for transparents
	
		totalContribution += ComputeAmbientLighting(ambientLightParams);
	}
	

	// Directional:
	{
		const uint numDirectionalLights = LightCounts.g_numLights.x;
		for (uint directionalIdx = 0; directionalIdx < numDirectionalLights; ++directionalIdx)
		{
			const LightShadowLUTData indexLUT = DirectionalLUT[directionalIdx];
			
			const uint lightBufferIdx = indexLUT.g_lightShadowIdx.x;
			const uint shadowBufferIdx = indexLUT.g_lightShadowIdx.y;
			const uint shadowTexIdx = indexLUT.g_lightShadowIdx.z;
			
			const LightData lightData = DirectionalLightParams[lightBufferIdx];
			
			float shadowFactor = 1.f;
			if (shadowBufferIdx != INVALID_SHADOW_IDX)
			{
				const ShadowData shadowData = ShadowParams[shadowBufferIdx];

				const bool shadowEnabled = shadowData.g_shadowParams.x;	
				if (shadowEnabled)
				{
#if defined(SHADOWS_RAYTRACED)
				shadowFactor = TraceShadowRay(
					SceneBVH,
					TraceRayInlineParams,
					worldPos,
					lightData.g_lightWorldPosRadius.xyz,
					TraceRayInlineParams.g_rayParams.x,
					FLT_MAX);
#else
					const float2 shadowCamNearFar = shadowData.g_shadowCamNearFarBiasMinMax.xy;
					const float2 minMaxShadowBias = shadowData.g_shadowCamNearFarBiasMinMax.zw;
					const float2 lightUVRadiusSize = shadowData.g_shadowParams.zw;
					const float shadowQualityMode = shadowData.g_shadowParams.y;
					
					shadowFactor = Get2DShadowFactor(
						worldPos,
						worldNormal,
						lightData.g_lightWorldPosRadius.xyz,
						shadowData.g_shadowCam_VP,
						shadowCamNearFar,
						minMaxShadowBias,
						shadowQualityMode,
						lightUVRadiusSize,
						shadowData.g_shadowMapTexelSize,
						DirectionalShadows,
						shadowTexIdx);
#endif
				}
			}			
		
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
		const uint numPointLights = LightCounts.g_numLights.y;
		for (uint pointIdx = 0; pointIdx < numPointLights; ++pointIdx)
		{
			const LightShadowLUTData indexLUT = PointLUT[pointIdx];
			
			const uint lightBufferIdx = indexLUT.g_lightShadowIdx.x;
			const uint shadowBufferIdx = indexLUT.g_lightShadowIdx.y;
			const uint shadowTexIdx = indexLUT.g_lightShadowIdx.z;
			
			const LightData lightData = PointLightParams[lightBufferIdx];
			
			const float3 lightWorldPos = lightData.g_lightWorldPosRadius.xyz;
			const float3 lightWorldDir = normalize(lightWorldPos - worldPos);
			
			// Convert luminous power (phi) to luminous intensity (I):
			const float luminousIntensity = lightData.g_lightColorIntensity.a * M_1_4PI;
			
			const float emitterRadius = lightData.g_lightWorldPosRadius.w;
			const float attenuationFactor = ComputeNonSingularAttenuationFactor(worldPos, lightWorldPos, emitterRadius);
			
			float shadowFactor = 1.f;
			if (shadowBufferIdx != INVALID_SHADOW_IDX)
			{			
				const ShadowData shadowData = ShadowParams[shadowBufferIdx];
					
				const bool shadowEnabled = shadowData.g_shadowParams.x > 0.f;
				if (shadowEnabled)
				{
#if defined(SHADOWS_RAYTRACED)
			const float rayLength = length(lightWorldPos - worldPos) - TraceRayInlineParams.g_rayParams.y;
					
			// Trace in reverse: Light -> world position, so we don't hit fake light source meshes
			shadowFactor = TraceShadowRay(
				SceneBVH,
				TraceRayInlineParams,
				lightWorldPos,
				-lightWorldDir,
				TraceRayInlineParams.g_rayParams.x,
				rayLength);
#else
					const float2 shadowCamNearFar = shadowData.g_shadowCamNearFarBiasMinMax.xy;
					const float2 minMaxShadowBias = shadowData.g_shadowCamNearFarBiasMinMax.zw;
					const float cubeFaceDimension = shadowData.g_shadowMapTexelSize.x; // Assume the cubemap width/height are the same
					const float2 lightUVRadiusSize = shadowData.g_shadowParams.zw;
					const float shadowQualityMode = shadowData.g_shadowParams.y;
				
					shadowFactor = GetCubeShadowFactor(
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
						shadowTexIdx);
#endif
				}
			}			
			
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
		const uint numSpotLights = LightCounts.g_numLights.z;
		for (uint spotIdx = 0; spotIdx < numSpotLights; ++spotIdx)
		{
			const LightShadowLUTData indexLUT = SpotLUT[spotIdx];
			
			const uint lightBufferIdx = indexLUT.g_lightShadowIdx.x;
			const uint shadowBufferIdx = indexLUT.g_lightShadowIdx.y;
			const uint shadowTexIdx = indexLUT.g_lightShadowIdx.z;
			
			const LightData lightData = SpotLightParams[lightBufferIdx];
			
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
			
			float shadowFactor = 1.f;
			if (shadowBufferIdx != INVALID_SHADOW_IDX)
			{
				const ShadowData shadowData = ShadowParams[shadowBufferIdx];
				
				const bool shadowEnabled = shadowData.g_shadowParams.x;
				if (shadowEnabled)
				{
#if defined(SHADOWS_RAYTRACED)
			const float rayLength = length(lightWorldPos - worldPos) - TraceRayInlineParams.g_rayParams.y;
			
			shadowFactor = TraceShadowRay(
				SceneBVH,
				TraceRayInlineParams,
				worldPos,
				lightWorldDir,
				TraceRayInlineParams.g_rayParams.x,
				rayLength);
#else
					const float2 shadowCamNearFar = shadowData.g_shadowCamNearFarBiasMinMax.xy;
					const float2 minMaxShadowBias = shadowData.g_shadowCamNearFarBiasMinMax.zw;
					const float2 lightUVRadiusSize = shadowData.g_shadowParams.zw;
					const float shadowQualityMode = shadowData.g_shadowParams.y;
					
					shadowFactor = Get2DShadowFactor(
						worldPos,
						worldNormal,
						lightWorldPos,
						shadowData.g_shadowCam_VP,
						shadowCamNearFar,
						minMaxShadowBias,
						shadowQualityMode,
						lightUVRadiusSize,
						shadowData.g_shadowMapTexelSize,
						SpotShadows,
						shadowTexIdx);
#endif
				}	
			}
	
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