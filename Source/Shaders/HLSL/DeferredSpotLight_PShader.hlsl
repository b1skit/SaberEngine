// © 2023 Adam Badke. All rights reserved.

#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "SaberCommon.hlsli"
#include "Shadows.hlsli"



// As per equation 64, section 5.2.2.2 of "Physically Based Rendering in Filament"
// https://google.github.io/filament/Filament.md.html#lighting/directlighting/punctuallights
float GetSpotlightAngleAttenuation(
	float3 toLight, 
	float3 lightWorldForwardDir, 
	float innerConeAngle, 
	float outerConeAngle, 
	float cosOuterAngle, 
	float scaleTerm,
	float offsetTerm)
{
	const float cd = dot(-toLight, lightWorldForwardDir);
	
	float attenuation = saturate(cd * scaleTerm + offsetTerm);
	
	return attenuation * attenuation; // Smooths the resulting transition
}


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.Position.xy);
	
	const float2 screenUV = PixelCoordsToScreenUV(In.Position.xy, TargetParams.g_targetDims.xy, float2(0.f, 0.f));

	const float3 worldPos = ScreenUVToWorldPos(screenUV, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection);
	const float3 lightWorldPos = LightParams.g_lightWorldPosRadius.xyz;
	
	const float3 lightWorldDir = normalize(lightWorldPos - worldPos);
	
	// Convert luminous power (phi) to luminous intensity (I):
	const float luminousIntensity = LightParams.g_lightColorIntensity.a * M_1_PI;
	
	const float emitterRadius = LightParams.g_lightWorldPosRadius.w;
	const float distanceAttenuation = ComputeNonSingularAttenuationFactor(worldPos, lightWorldPos, emitterRadius);
	
	const float angleAttenuation = GetSpotlightAngleAttenuation(
		lightWorldDir,
		LightParams.g_globalForwardDir.xyz,
		LightParams.g_intensityScale.z,
		LightParams.g_intensityScale.w,
		LightParams.g_extraParams.x,
		LightParams.g_extraParams.y,
		LightParams.g_extraParams.z);
	
	const float combinedAttenuation = distanceAttenuation * angleAttenuation;
	
	const float2 shadowCamNearFar = LightParams.g_shadowCamNearFarBiasMinMax.xy;
	const float2 minMaxShadowBias = LightParams.g_shadowCamNearFarBiasMinMax.zw;
	const float2 invShadowMapWidthHeight = LightParams.g_shadowMapTexelSize.zw;
	const float2 lightUVRadiusSize = LightParams.g_shadowParams.zw;
	const float shadowQualityMode = LightParams.g_shadowParams.y;
	
	const bool shadowEnabled = LightParams.g_shadowParams.x > 0.f;
	const float shadowFactor = shadowEnabled ?
		Get2DShadowFactor(
			worldPos,
			gbuffer.WorldNormal,
			lightWorldPos,
			LightParams.g_shadowCam_VP,
			shadowCamNearFar,
			minMaxShadowBias,
			shadowQualityMode,
			lightUVRadiusSize,
			invShadowMapWidthHeight) : 1.f;
	
	const float NoL = saturate(dot(gbuffer.WorldNormal, lightWorldDir));
	
	LightingParams lightingParams;
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.RemappedRoughness = RemapRoughness(gbuffer.LinearRoughness);
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	lightingParams.WorldPosition = worldPos;
	lightingParams.F0 = gbuffer.MatProp0.rgb;
	
	lightingParams.NoL = NoL;
	
	lightingParams.LightWorldPos = lightWorldPos;
	lightingParams.LightWorldDir = lightWorldDir;
	lightingParams.LightColor = LightParams.g_lightColorIntensity.rgb;
	lightingParams.LightIntensity = luminousIntensity;
	lightingParams.LightAttenuationFactor = combinedAttenuation;
	
	lightingParams.ShadowFactor = shadowFactor;
	
	lightingParams.CameraWorldPos = CameraParams.g_cameraWPos.xyz;
	lightingParams.Exposure = CameraParams.g_exposureProperties.x;
	
	
	lightingParams.DiffuseScale = LightParams.g_intensityScale.x;
	lightingParams.SpecularScale = LightParams.g_intensityScale.y;
	
	return float4(ComputeLighting(lightingParams), 0.f);
}