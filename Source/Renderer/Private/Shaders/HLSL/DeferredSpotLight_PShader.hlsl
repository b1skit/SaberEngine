// © 2023 Adam Badke. All rights reserved.
#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "SaberCommon.hlsli"
#include "Shadows.hlsli"

#include "../Common/LightParams.h"
#include "../Common/MaterialParams.h"
#include "../Common/ShadowParams.h"
#include "../Common/TargetParams.h"


ConstantBuffer<TargetData> TargetParams;

StructuredBuffer<LightShadowLUTData> SpotLUT;

StructuredBuffer<LightData> SpotLightParams;
StructuredBuffer<ShadowData> ShadowParams;

Texture2DArray<float> SpotShadows;


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.Position.xy);
	
	if (gbuffer.MaterialID != MAT_ID_GLTF_PBRMetallicRoughness)
	{
		return float4(0.f, 0.f, 0.f, 0.f);
	}
	
	const float2 screenUV = PixelCoordsToScreenUV(In.Position.xy, TargetParams.g_targetDims.xy, float2(0.f, 0.f));
	
	const LightShadowLUTData indexLUT = SpotLUT[0];
	
	const uint lightBufferIdx = indexLUT.g_lightShadowIdx.x;
	const uint shadowBufferIdx = indexLUT.g_lightShadowIdx.y;
	const uint shadowTexIdx = indexLUT.g_lightShadowIdx.z;
			
	const LightData lightData = SpotLightParams[lightBufferIdx];	
	
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
				SpotShadows,
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