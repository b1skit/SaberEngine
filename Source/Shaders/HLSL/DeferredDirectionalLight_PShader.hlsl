// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "MathConstants.hlsli"
#include "SaberCommon.hlsli"
#include "Shadows.hlsli"
#include "Transformations.hlsli"

Texture2DArray<float> DirectionalShadows;


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.Position.xy);
	
	const float3 worldPos = ScreenUVToWorldPos(In.UV0, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection);
	
	const uint lightParamsIdx = LightIndexParams.g_lightIndex.x;
	const uint shadowIdx = LightIndexParams.g_lightIndex.y;
	
	const LightData lightData = DirectionalLightParams[lightParamsIdx];
	
	const float2 shadowCamNearFar = lightData.g_shadowCamNearFarBiasMinMax.xy;
	const float2 minMaxShadowBias = lightData.g_shadowCamNearFarBiasMinMax.zw;
	const float2 lightUVRadiusSize = lightData.g_shadowParams.zw;
	const float shadowQualityMode = lightData.g_shadowParams.y;
	
	const bool shadowEnabled = lightData.g_shadowParams.x > 0.f;
	const float shadowFactor = shadowEnabled ?
		Get2DShadowFactor(
			worldPos,
			gbuffer.WorldNormal,
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
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.RemappedRoughness = RemapRoughness(gbuffer.LinearRoughness);
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	lightingParams.WorldPosition = worldPos;
	lightingParams.F0 = gbuffer.MatProp0.rgb;
	
	const float NoL = saturate(dot(gbuffer.WorldNormal, lightData.g_lightWorldPosRadius.xyz));
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
	
	return float4(ComputeLighting(lightingParams), 0.f);
}