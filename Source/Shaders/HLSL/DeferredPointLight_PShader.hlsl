// © 2023 Adam Badke. All rights reserved.

#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "SaberCommon.hlsli"
#include "Shadows.hlsli"


// As per Cem Yuksel's nonsingular point light attenuation function:
// http://www.cemyuksel.com/research/pointlightattenuation/
float ComputePointLightAttenuationFactor(float3 worldPos, float3 lightPos, float emitterRadius)
{
	const float r2 = emitterRadius * emitterRadius;
	
	const float lightDistance = length(worldPos - lightPos);
	const float d2 = lightDistance * lightDistance;
	
	const float attenuation = 2.f / (d2 + r2 + (lightDistance * sqrt(d2 + r2)));
	
	return attenuation;
}


float4 PShader(VertexOut In) : SV_Target
{
	const float2 screenUV = PixelCoordsToUV(In.Position.xy, LightParams.g_renderTargetResolution.xy, float2(0.f, 0.f));
	const GBuffer gbuffer = UnpackGBuffer(screenUV);

	const float3 worldPos = GetWorldPos(screenUV, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection);
	const float3 lightWorldPos = LightParams.g_lightWorldPosRadius.xyz;
	
	const float3 lightWorldDir = normalize(LightParams.g_lightWorldPosRadius.xyz - worldPos);
	
	// As per equation 15 (p.29) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al, we can convert
	// a point light's luminous power I (lm) to luminous intensity (phi) using phi = 4pi * I. However, we then normalize
	// this over the solid angle 4pi, thus we can ignore the 4pi term
	const float luminousIntensity = LightParams.g_lightColorIntensity.a;
	
	const float emitterRadius = LightParams.g_lightWorldPosRadius.w;
	const float attenuationFactor = ComputePointLightAttenuationFactor(worldPos, lightWorldPos, emitterRadius);
	
	const float2 shadowCamNearFar = LightParams.g_shadowCamNearFarBiasMinMax.xy;
	const float2 minMaxShadowBias = LightParams.g_shadowCamNearFarBiasMinMax.zw;
	const float cubeFaceDimension = LightParams.g_shadowMapTexelSize.x; // Assume the cubemap width/height are the same
	const float2 lightUVRadiusSize = LightParams.g_shadowParams.zw;
	const float shadowQualityMode = LightParams.g_shadowParams.y;
	
	const bool shadowEnabled = LightParams.g_shadowParams.x > 0.f;
	const float shadowFactor = shadowEnabled ? 
		GetCubeShadowFactor(
			worldPos, 
			gbuffer.WorldNormal,
			lightWorldPos, 
			lightWorldDir, 
			shadowCamNearFar, 
			minMaxShadowBias,
			shadowQualityMode,
			lightUVRadiusSize,
			cubeFaceDimension) : 1.f;
	
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
	lightingParams.LightAttenuationFactor = attenuationFactor;
	
	lightingParams.ShadowFactor = shadowFactor;
	
	lightingParams.CameraWorldPos = CameraParams.g_cameraWPos.xyz;
	lightingParams.Exposure = CameraParams.g_exposureProperties.x;
	
	
	lightingParams.DiffuseScale = LightParams.g_intensityScale.x;
	lightingParams.SpecularScale = LightParams.g_intensityScale.y;
	
	return float4(ComputeLighting(lightingParams), 0.f);
}