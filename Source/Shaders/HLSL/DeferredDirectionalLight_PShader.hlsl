// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "MathConstants.hlsli"
#include "SaberCommon.hlsli"
#include "Shadows.hlsli"
#include "Transformations.hlsli"


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.UV0);
	
	const float3 worldPos = GetWorldPos(In.UV0, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection);
	
	const float2 minMaxShadowBias = LightParams.g_shadowCamNearFarBiasMinMax.zw;
	const float2 invShadowMapWidthHeight = LightParams.g_shadowMapTexelSize.zw;
	
	const bool shadowEnabled = LightParams.g_intensityScaleShadowed.z > 0.f;
	const float shadowFactor = shadowEnabled ?
		Get2DShadowFactor(
			worldPos,
			gbuffer.WorldNormal,
			LightParams.g_lightWorldPosRadius.xyz,
			LightParams.g_shadowCam_VP,
			minMaxShadowBias,
			invShadowMapWidthHeight) : 1.f;
	
	const float NoL = saturate(dot(gbuffer.WorldNormal, LightParams.g_lightWorldPosRadius.xyz));
	
	LightingParams lightingParams;
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.RemappedRoughness = RemapRoughness(gbuffer.LinearRoughness);
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	lightingParams.WorldPosition = float3(0.f, 0.f, 0.f); // Directional lights are at infinity
	lightingParams.F0 = gbuffer.MatProp0.rgb;
	
	lightingParams.NoL = NoL;
	
	lightingParams.LightWorldPos = worldPos; // Ensure attenuation = 0
	lightingParams.LightWorldDir = LightParams.g_lightWorldPosRadius.xyz; 
	lightingParams.LightColor = LightParams.g_lightColorIntensity.rgb;
	lightingParams.LightIntensity = LightParams.g_lightColorIntensity.a;
	lightingParams.LightAttenuationFactor = 1.f;
	
	lightingParams.ShadowFactor = shadowFactor;
	
	lightingParams.CameraWorldPos = CameraParams.g_cameraWPos.xyz;
	lightingParams.Exposure = CameraParams.g_exposureProperties.x;
	
	lightingParams.DiffuseScale = LightParams.g_intensityScaleShadowed.x;
	lightingParams.SpecularScale = LightParams.g_intensityScaleShadowed.y;
	
	return float4(ComputeLighting(lightingParams), 0.f);
}