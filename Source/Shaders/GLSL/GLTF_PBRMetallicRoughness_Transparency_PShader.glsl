// © 2023 Adam Badke. All rights reserved.
#version 460 core
#define SABER_VEC4_OUTPUT
#define VOUT_COLOR
#define VOUT_TBN
#define SABER_INSTANCING

#include "SaberCommon.glsli"

#include "AmbientCommon.glsli"
#include "NormalMapUtils.glsli"
#include "Shadows.glsli"

#include "../Common/CameraParams.h"
#include "../Common/InstancingParams.h"
#include "../Common/LightParams.h"
#include "../Common/MaterialParams.h"
#include "../Common/ShadowParams.h"


layout(std430, binding = 0) readonly buffer InstanceIndexParams { InstanceIndexData _InstanceIndexParams[]; };

layout(std430, binding = 2) readonly buffer InstancedPBRMetallicRoughnessParams {	PBRMetallicRoughnessData _InstancedPBRMetallicRoughnessParams[]; };

layout(std430, binding = 3) readonly buffer DirectionalLightParams { LightData _DirectionalLightParams[]; };
layout(std430, binding = 4) readonly buffer PointLightParams { LightData _PointLightParams[]; };
layout(std430, binding = 5) readonly buffer SpotLightParams { LightData _SpotLightParams[]; };

layout(std430, binding = 6) readonly buffer DirectionalLUT { LightShadowLUTData _DirectionalLUT[]; };
layout(std430, binding = 7) readonly buffer PointLUT { LightShadowLUTData _PointLUT[]; };
layout(std430, binding = 8) readonly buffer SpotLUT { LightShadowLUTData _SpotLUT[]; };

layout(std430, binding = 9) readonly buffer ShadowParams { ShadowData _ShadowParams[]; };

layout(binding = 10) uniform sampler2DArrayShadow DirectionalShadows;
layout(binding = 11) uniform sampler2DArrayShadow SpotShadows;
layout(binding = 12) uniform samplerCubeArrayShadow PointShadows;

layout(binding = 7) uniform CameraParams { CameraData _CameraParams; };

layout(binding = 8) uniform LightCounts { LightMetadata _LightCounts; };

layout(binding = 0) uniform sampler2D BaseColorTex;
layout(binding = 1) uniform sampler2D MetallicRoughnessTex;
layout(binding = 2) uniform sampler2D NormalTex;
layout(binding = 3) uniform sampler2D OcclusionTex;


void PShader()
{
	const uint materialIdx = _InstanceIndexParams[InstanceParamsIn.InstanceID].g_indexes.y;

	const vec2 albedoUV = GetUV(In, 
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_uvChannelIndexes0.x);
	
	const vec2 metallicRoughnessUV = GetUV(In,
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_uvChannelIndexes0.y);
	
	const vec2 normalUV = GetUV(In,
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_uvChannelIndexes0.z);
	
	const vec2 occlusionUV = GetUV(In,
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_uvChannelIndexes0.w);
	
	//const vec2 emissiveUV = GetUV(In,
	//	_InstancedPBRMetallicRoughnessParams[materialIdx].g_uvChannelIndexes1.x);

	const vec3 worldPos = In.WorldPos;

	const vec4 matAlbedo = texture(BaseColorTex, albedoUV);
	const vec4 baseColorFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor;
	const vec3 linearAlbedo = (matAlbedo * baseColorFactor * In.Color).rgb;

	const float normalScaleFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.z;
	const vec3 normalScale = vec3(normalScaleFactor, normalScaleFactor, 1.f);
	const vec3 texNormal = texture(NormalTex, normalUV).xyz;
	const vec3 worldNormal = WorldNormalFromTextureNormal(texNormal, In.TBN) * normalScale;

	const float linearRoughnessFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.y;
	
	const float metallicFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.x;
	
	const vec2 roughnessMetalness =
		texture(MetallicRoughnessTex, metallicRoughnessUV).gb * vec2(linearRoughnessFactor, metallicFactor);

	const float remappedRoughness = RemapRoughness(roughnessMetalness.x);

	const vec3 f0 = _InstancedPBRMetallicRoughnessParams[materialIdx].g_f0AlphaCutoff.rgb;

	vec3 totalContribution = vec3(0.f);

	// Ambient:
	{
		AmbientLightingParams ambientLightParams;
	
		ambientLightParams.WorldPosition = worldPos;
		ambientLightParams.V = normalize(_CameraParams.g_cameraWPos.xyz - worldPos); // point -> camera
		ambientLightParams.WorldNormal = worldNormal;
	
		ambientLightParams.LinearAlbedo = linearAlbedo;
		ambientLightParams.DielectricSpecular = f0;
	
		ambientLightParams.LinearMetalness = roughnessMetalness.y;
		ambientLightParams.LinearRoughness = roughnessMetalness.x;
		ambientLightParams.RemappedRoughness = remappedRoughness;

		const float occlusionStrength =
			_InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.w;
		const float occlusion = texture(OcclusionTex, occlusionUV).r * occlusionStrength;
	
		ambientLightParams.FineAO = occlusion;
		ambientLightParams.CoarseAO = 1.f; // No SSAO for transparents

		totalContribution += ComputeAmbientLighting(ambientLightParams);
	}

	// Directional:
	{
		const uint numDirectionalLights = _LightCounts.g_numLights.x;
		for (uint directionalIdx = 0; directionalIdx < numDirectionalLights; ++directionalIdx)
		{
			const LightShadowLUTData indexLUT = _DirectionalLUT[directionalIdx];

			const uint lightBufferIdx = indexLUT.g_lightShadowIdx.x;
			const uint shadowBufferIdx = indexLUT.g_lightShadowIdx.y;
			const uint shadowTexIdx = indexLUT.g_lightShadowIdx.z;
			
			const LightData lightData = _DirectionalLightParams[lightBufferIdx];

			float shadowFactor = 1.f;
			if (shadowBufferIdx != INVALID_SHADOW_IDX)
			{
				const ShadowData shadowData = _ShadowParams[shadowBufferIdx];

				const bool shadowEnabled = shadowData.g_shadowParams.x > 0.f;	
				if (shadowEnabled)
				{
					const vec2 shadowCamNearFar = shadowData.g_shadowCamNearFarBiasMinMax.xy;
					const vec2 minMaxShadowBias = shadowData.g_shadowCamNearFarBiasMinMax.zw;
					const vec2 lightUVRadiusSize = shadowData.g_shadowParams.zw;
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
				}
			}

			LightingParams lightingParams;
			lightingParams.LinearAlbedo =  linearAlbedo;
			lightingParams.WorldNormal = worldNormal;
			lightingParams.LinearRoughness = roughnessMetalness.x;
			lightingParams.RemappedRoughness = remappedRoughness;
			lightingParams.LinearMetalness = roughnessMetalness.y;
			lightingParams.WorldPosition = worldPos;
			lightingParams.F0 = f0;

			const float NoL = max(0.0, dot(worldNormal, lightData.g_lightWorldPosRadius.xyz));
			lightingParams.NoL = NoL;

			lightingParams.LightWorldPos = worldPos; // Ensure attenuation = 0
			lightingParams.LightWorldDir = lightData.g_lightWorldPosRadius.xyz;
			lightingParams.LightColor = lightData.g_lightColorIntensity.rgb;
			lightingParams.LightIntensity = lightData.g_lightColorIntensity.a;
			lightingParams.LightAttenuationFactor = 1.f;
			lightingParams.ShadowFactor = shadowFactor;

			lightingParams.CameraWorldPos = _CameraParams.g_cameraWPos.xyz;
			lightingParams.Exposure = _CameraParams.g_exposureProperties.x;

			lightingParams.DiffuseScale = lightData.g_intensityScale.x;
			lightingParams.SpecularScale = lightData.g_intensityScale.y;

			totalContribution += ComputeLighting(lightingParams);
		}
	}

	// Point:
	{
		const uint numPointLights = _LightCounts.g_numLights.y;
		for (uint pointIdx = 0; pointIdx < numPointLights; ++pointIdx)
		{
			const LightShadowLUTData indexLUT = _PointLUT[pointIdx];
			
			const uint lightBufferIdx = indexLUT.g_lightShadowIdx.x;
			const uint shadowBufferIdx = indexLUT.g_lightShadowIdx.y;
			const uint shadowTexIdx = indexLUT.g_lightShadowIdx.z;
			
			const LightData lightData = _PointLightParams[lightBufferIdx];

			const vec3 lightWorldPos = lightData.g_lightWorldPosRadius.xyz;
			const vec3 lightWorldDir = normalize(lightWorldPos - worldPos.xyz);

			// Convert luminous power (phi) to luminous intensity (I):
			const float luminousIntensity = lightData.g_lightColorIntensity.a * M_1_4PI;

			const float emitterRadius = lightData.g_lightWorldPosRadius.w;
			const float attenuationFactor = ComputeNonSingularAttenuationFactor(worldPos, lightWorldPos, emitterRadius);

			float shadowFactor = 1.f;
			if (shadowBufferIdx != INVALID_SHADOW_IDX)
			{			
				const ShadowData shadowData = _ShadowParams[shadowBufferIdx];
	
				const bool shadowEnabled = shadowData.g_shadowParams.x > 0.f;
				if (shadowEnabled)
				{
					const vec2 shadowCamNearFar = shadowData.g_shadowCamNearFarBiasMinMax.xy;
					const vec2 minMaxShadowBias = shadowData.g_shadowCamNearFarBiasMinMax.zw;
					const float cubeFaceDimension = shadowData.g_shadowMapTexelSize.x; // Assume the cubemap width/height are the same
					const vec2 lightUVRadiusSize = shadowData.g_shadowParams.zw;
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

			const float NoL = clamp(dot(worldNormal, lightWorldDir), 0.f, 1.f);
			lightingParams.NoL = NoL;

			lightingParams.LightWorldPos = lightWorldPos;
			lightingParams.LightWorldDir = lightWorldDir;
			lightingParams.LightColor = lightData.g_lightColorIntensity.rgb;
			lightingParams.LightIntensity = luminousIntensity;
			lightingParams.LightAttenuationFactor = attenuationFactor;
			
			lightingParams.ShadowFactor = shadowFactor;
	
			lightingParams.CameraWorldPos = _CameraParams.g_cameraWPos.xyz;
			lightingParams.Exposure = _CameraParams.g_exposureProperties.x;

			lightingParams.DiffuseScale = lightData.g_intensityScale.x;
			lightingParams.SpecularScale = lightData.g_intensityScale.y;

			totalContribution += ComputeLighting(lightingParams);
		}
	}

	// Spot:
	{
		const uint numSpotLights = _LightCounts.g_numLights.z;
		for (uint spotIdx = 0; spotIdx < numSpotLights; ++spotIdx)
		{
			const LightShadowLUTData indexLUT = _SpotLUT[spotIdx];
			
			const uint lightBufferIdx = indexLUT.g_lightShadowIdx.x;
			const uint shadowBufferIdx = indexLUT.g_lightShadowIdx.y;
			const uint shadowTexIdx = indexLUT.g_lightShadowIdx.z;
			
			const LightData lightData = _SpotLightParams[lightBufferIdx];

			const vec3 lightWorldPos = lightData.g_lightWorldPosRadius.xyz;
			const vec3 lightWorldDir = normalize(lightWorldPos - worldPos.xyz);

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
				const ShadowData shadowData = _ShadowParams[shadowBufferIdx];
				
				const bool shadowEnabled = shadowData.g_shadowParams.x > 0.f;
				if (shadowEnabled)
				{
					const vec2 shadowCamNearFar = shadowData.g_shadowCamNearFarBiasMinMax.xy;
					const vec2 minMaxShadowBias = shadowData.g_shadowCamNearFarBiasMinMax.zw;
					const vec2 lightUVRadiusSize = shadowData.g_shadowParams.zw;
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

			const float NoL = clamp(dot(worldNormal, lightWorldDir), 0.f, 1.f);
			lightingParams.NoL = NoL;

			lightingParams.LightWorldPos = lightWorldPos;
			lightingParams.LightWorldDir = lightWorldDir;
			lightingParams.LightColor = lightData.g_lightColorIntensity.rgb;
			lightingParams.LightIntensity = luminousIntensity;
			lightingParams.LightAttenuationFactor = combinedAttenuation;
			lightingParams.ShadowFactor = shadowFactor;
	
			lightingParams.CameraWorldPos = _CameraParams.g_cameraWPos.xyz;
			lightingParams.Exposure = _CameraParams.g_exposureProperties.x;

			lightingParams.DiffuseScale = lightData.g_intensityScale.x;
			lightingParams.SpecularScale = lightData.g_intensityScale.y;

			totalContribution += ComputeLighting(lightingParams);
		}
	}

	FragColor = vec4(totalContribution, matAlbedo.a);
}