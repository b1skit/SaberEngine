// © 2023 Adam Badke. All rights reserved.
#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "SaberCommon.hlsli"
#include "Shadows.hlsli"

#include "../Common/CameraParams.h"
#include "../Common/MaterialParams.h"
#include "../Common/ShadowParams.h"
#include "../Common/TargetParams.h"

ConstantBuffer<CameraData> CameraParams : register(space1);
ConstantBuffer<TargetData> TargetParams;

StructuredBuffer<LightShadowLUTData> PointLUT;

TextureCubeArray<float> PointShadows;

StructuredBuffer<LightData> PointLightParams;
StructuredBuffer<ShadowData> ShadowParams;


#if defined(SHADOWS_RAYTRACED)
#include "RayTracingCommon.hlsli"
#include "../Common/RayTracingParams.h"

RaytracingAccelerationStructure SceneBVH;
ConstantBuffer<TraceRayInlineData> TraceRayInlineParams;
#endif


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.Position.xy);
	
	if (gbuffer.MaterialID != MAT_ID_GLTF_PBRMetallicRoughness)
	{
		return float4(0.f, 0.f, 0.f, 0.f);
	}
	
	const float2 screenUV = PixelCoordsToScreenUV(In.Position.xy, TargetParams.g_targetDims.xy, float2(0.f, 0.f));
	
	const LightShadowLUTData indexLUT = PointLUT[0];
			
	const uint lightBufferIdx = indexLUT.g_lightShadowIdx.x;
	const uint shadowBufferIdx = indexLUT.g_lightShadowIdx.y;
	const uint shadowTexIdx = indexLUT.g_lightShadowIdx.z;
			
	const LightData lightData = PointLightParams[lightBufferIdx];
	
	const float3 worldPos = ScreenUVToWorldPos(screenUV, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection);
	
	const float3 lightWorldPos = lightData.g_lightWorldPosRadius.xyz;	
	const float3 lightWorldDir = normalize(lightWorldPos - worldPos);
	
	// Convert luminous power (phi) to luminous intensity (I):
	const float luminousIntensity = lightData.g_lightColorIntensity.a * M_1_4PI;
	
	const float emitterRadius = lightData.g_lightWorldPosRadius.w;
	const float attenuationFactor = ComputeNonSingularAttenuationFactor(worldPos, lightWorldPos, emitterRadius);
	
	float shadowFactor = 1.f;
	if (shadowBufferIdx != INVALID_SHADOW_IDX)
	{
		const ShadowData shadowData = ShadowParams[shadowBufferIdx];
					
		const bool shadowEnabled = shadowData.g_shadowParams.x > 0.f;
		if (shadowEnabled)
		{
#if defined(SHADOWS_RAYTRACED)
			const float rayLength = length(lightWorldPos - worldPos) - TraceRayInlineParams.g_rayParams.y;
			
			// Trace in reverse: Light -> world position, so we don't hit fake light source meshes
			shadowFactor = TraceShadowRayInline(
				SceneBVH,
				TraceRayInlineParams,
				lightWorldPos,
				-lightWorldDir,
				gbuffer.WorldVertexNormal,
				TraceRayInlineParams.g_rayParams.x,
				rayLength);
#else
			const float2 shadowCamNearFar = shadowData.g_shadowCamNearFarBiasMinMax.xy;
			const float2 minMaxShadowBias = shadowData.g_shadowCamNearFarBiasMinMax.zw;
			const float cubeFaceDimension = shadowData.g_shadowMapTexelSize.x; // Assume the cubemap width/height are the same
			const float2 lightUVRadiusSize = shadowData.g_shadowParams.zw;
			const float shadowQualityMode = shadowData.g_shadowParams.y;
				
			shadowFactor = GetCubeShadowFactor(
				worldPos,
				gbuffer.WorldNormal,
				lightWorldPos,
				lightWorldDir,
				shadowCamNearFar,
				minMaxShadowBias,
				shadowQualityMode,
				lightUVRadiusSize,
				cubeFaceDimension,
				PointShadows,
				shadowTexIdx);
#endif
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
	lightingParams.LightAttenuationFactor = attenuationFactor;
	
	lightingParams.ShadowFactor = shadowFactor;
	
	lightingParams.CameraWorldPos = CameraParams.g_cameraWPos.xyz;
	lightingParams.Exposure = CameraParams.g_exposureProperties.x;
	
	lightingParams.DiffuseScale = lightData.g_intensityScale.x;
	lightingParams.SpecularScale = lightData.g_intensityScale.y;
	
	return float4(ComputeLighting(lightingParams), 0.f);
}