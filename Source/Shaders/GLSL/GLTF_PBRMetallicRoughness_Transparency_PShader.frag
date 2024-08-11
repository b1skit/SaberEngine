// © 2023 Adam Badke. All rights reserved.
#version 460 core
#define SABER_VEC4_OUTPUT
#define VOUT_COLOR
#define VOUT_TBN
#define VOUT_WORLD_POS
#define SABER_INSTANCING

#include "SaberCommon.glsl"

#include "AmbientCommon.glsl"
#include "NormalMapUtils.glsl"
#include "Shadows.glsl"


uint UnpackPointLightIndex(uint arrayIdx)
{
	const uint elementIdx = arrayIdx % 4;
	const uint vec4ArrayIdx = arrayIdx / 4;

	return _AllLightIndexesParams.g_pointIndexes[vec4ArrayIdx][elementIdx];
}


uint UnpackSpotLightIndex(uint arrayIdx)
{
	const uint elementIdx = arrayIdx % 4;
	const uint vec4ArrayIdx = arrayIdx / 4;

	return _AllLightIndexesParams.g_spotIndexes[vec4ArrayIdx][elementIdx];
}


void main()
{
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[InstanceParamsIn.InstanceID].g_materialIdx;

	const vec3 worldPos = In.WorldPos;

	const vec4 matAlbedo = texture(MatAlbedo, In.UV0.xy);
	const vec4 baseColorFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor;
	const vec3 linearAlbedo = (matAlbedo * baseColorFactor * In.Color).rgb;

	const float normalScaleFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.z;
	const vec3 normalScale = vec3(normalScaleFactor, normalScaleFactor, 1.f);
	const vec3 texNormal = texture(MatNormal, In.UV0.xy).xyz;
	const vec3 worldNormal = WorldNormalFromTextureNormal(texNormal, In.TBN) * normalScale;

	const float linearRoughnessFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.y;
	
	const float metallicFactor =
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.x;
	
	const vec2 roughnessMetalness =
		texture(MatMetallicRoughness, In.UV0.xy).gb * vec2(linearRoughnessFactor, metallicFactor);

	const float remappedRoughness = RemapRoughness(roughnessMetalness.x);

	const vec3 f0 = _InstancedPBRMetallicRoughnessParams[materialIdx].g_f0.rgb;

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
		const float occlusion = texture(MatOcclusion, In.UV0.xy).r * occlusionStrength;
	
		ambientLightParams.FineAO = occlusion;
		ambientLightParams.CoarseAO = 1.f; // No SSAO for transparents

		totalContribution += ComputeAmbientLighting(ambientLightParams);
	}

	// Directional:
	{
		const uint numDirectionalLights = _AllLightIndexesParams.g_numLights.x;

		for (uint directionalIdx = 0; directionalIdx < numDirectionalLights; ++directionalIdx)
		{
			const LightData lightData = _DirectionalLightParams[directionalIdx];

			const uint shadowIdx = uint(lightData.g_extraParams.w);

			const vec2 shadowCamNearFar = lightData.g_shadowCamNearFarBiasMinMax.xy;
			const vec2 minMaxShadowBias = lightData.g_shadowCamNearFarBiasMinMax.zw;
			const vec2 lightUVRadiusSize = lightData.g_shadowParams.zw;
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
		const uint numPointLights = _AllLightIndexesParams.g_numLights.y;

		for (uint pointIdx = 0; pointIdx < numPointLights; ++pointIdx)
		{
			const uint pointLightDataIdx = UnpackPointLightIndex(pointIdx);
			
			const LightData lightData = _PointLightParams[pointLightDataIdx];

			const uint shadowIdx = uint(lightData.g_extraParams.w);

			const vec3 lightWorldPos = lightData.g_lightWorldPosRadius.xyz;
			const vec3 lightWorldDir = normalize(lightWorldPos - worldPos.xyz);

			// Convert luminous power (phi) to luminous intensity (I):
			const float luminousIntensity = lightData.g_lightColorIntensity.a * M_1_4PI;

			const float emitterRadius = lightData.g_lightWorldPosRadius.w;
			const float attenuationFactor = ComputeNonSingularAttenuationFactor(worldPos, lightWorldPos, emitterRadius);

			const vec2 shadowCamNearFar = lightData.g_shadowCamNearFarBiasMinMax.xy;
			const vec2 minMaxShadowBias = lightData.g_shadowCamNearFarBiasMinMax.zw;
			const float cubeFaceDimension = lightData.g_shadowMapTexelSize.x; // Assume the cubemap width/height are the same
			const vec2 lightUVRadiusSize = lightData.g_shadowParams.zw;
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
		const uint numSpotLights = _AllLightIndexesParams.g_numLights.z;

		for (uint spotIdx = 0; spotIdx < numSpotLights; ++spotIdx)
		{
			const uint spotLightDataIdx = UnpackSpotLightIndex(spotIdx);
			
			const LightData lightData = _SpotLightParams[spotLightDataIdx];

			const uint shadowIdx = uint(lightData.g_extraParams.w);

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

			const vec2 shadowCamNearFar = lightData.g_shadowCamNearFarBiasMinMax.xy;
			const vec2 minMaxShadowBias = lightData.g_shadowCamNearFarBiasMinMax.zw;
			const vec2 lightUVRadiusSize = lightData.g_shadowParams.zw;
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