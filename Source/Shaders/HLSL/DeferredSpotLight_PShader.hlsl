// © 2023 Adam Badke. All rights reserved.
#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "SaberCommon.hlsli"
#include "Shadows.hlsli"

#include "../Common/TargetParams.h"

ConstantBuffer<TargetData> TargetParams;


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.Position.xy);
	
	const float2 screenUV = PixelCoordsToScreenUV(In.Position.xy, TargetParams.g_targetDims.xy, float2(0.f, 0.f));

	const uint lightParamsIdx = LightIndexParams.g_lightIndex.x;
	const uint shadowIdx = LightIndexParams.g_lightIndex.y;
	
	const LightData lightData = SpotLightParams[lightParamsIdx];
	
	const float3 worldPos = ScreenUVToWorldPos(screenUV, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection);
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
			gbuffer.WorldNormal,
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
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.RemappedRoughness = RemapRoughness(gbuffer.LinearRoughness);
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	lightingParams.WorldPosition = worldPos;
	lightingParams.F0 = gbuffer.MatProp0.rgb;
	
	const float NoL = saturate(dot(gbuffer.WorldNormal, lightWorldDir));	
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
	
	return float4(ComputeLighting(lightingParams), 0.f);
}