// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "MathConstants.hlsli"
#include "SaberCommon.hlsli"
#include "Transformations.hlsli"


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.UV0);
	
	const float4 worldPos = float4(GetWorldPos(In.UV0, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection), 1.f);
	
	LightingParams lightingParams;
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.RemappedRoughness = RemapRoughness(gbuffer.LinearRoughness);
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	lightingParams.WorldPosition = float3(0.f, 0.f, 0.f); // Directional lights are at infinity
	lightingParams.F0 = gbuffer.MatProp0.rgb;
	lightingParams.LightWorldPos = worldPos.xyz; // Ensure attenuation = 0
	lightingParams.LightWorldDir = LightParams.g_lightWorldPosRadius.xyz; 
	lightingParams.LightColor = LightParams.g_lightColorIntensity.rgb;
	lightingParams.LightIntensity = LightParams.g_lightColorIntensity.a;
	lightingParams.LightAttenuationFactor = 1.f;
	lightingParams.ShadowFactor = 1.f; // TODO: Compute this
	
	lightingParams.CameraWorldPos = CameraParams.g_cameraWPos;
	lightingParams.Exposure = CameraParams.g_exposureProperties.x;
	
	lightingParams.DiffuseScale = LightParams.g_intensityScale.x;
	lightingParams.SpecularScale = LightParams.g_intensityScale.y;
	
	return float4(ComputeLighting(lightingParams), 0.f);
}