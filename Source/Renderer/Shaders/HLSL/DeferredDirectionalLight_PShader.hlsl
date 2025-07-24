// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#include "CameraCommon.hlsli"
#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "MathConstants.hlsli"
#include "SaberCommon.hlsli"
#include "Shadows.hlsli"
#include "Transformations.hlsli"

#include "../Common/MaterialParams.h"

StructuredBuffer<LightData> DirectionalLightParams;
StructuredBuffer<LightShadowLUTData> DirectionalLUT;
StructuredBuffer<ShadowData> ShadowParams;

Texture2DArray<float> DirectionalShadows;


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.Position.xy);
	
	if (gbuffer.MaterialID != MAT_ID_GLTF_PBRMetallicRoughness)
	{
		return float4(0.f, 0.f, 0.f, 0.f);
	}
	
	const float3 worldPos = ScreenUVToWorldPos(In.UV0, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection);
	
	const LightShadowLUTData indexLUT = DirectionalLUT[0];
	
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
			const float2 shadowCamNearFar = shadowData.g_shadowCamNearFarBiasMinMax.xy;
			const float2 minMaxShadowBias = shadowData.g_shadowCamNearFarBiasMinMax.zw;
			const float2 lightUVRadiusSize = shadowData.g_shadowParams.zw;
			const float shadowQualityMode = shadowData.g_shadowParams.y;
					
			shadowFactor = Get2DShadowFactor(
				worldPos,
				gbuffer.WorldNormal,
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