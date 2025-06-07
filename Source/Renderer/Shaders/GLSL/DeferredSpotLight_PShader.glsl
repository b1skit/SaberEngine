// © 2023 Adam Badke. All rights reserved.
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsli"
#include "Lighting.glsli"
#include "Shadows.glsli"
#include "GBufferCommon.glsli"

#include "../Common/CameraParams.h"
#include "../Common/LightParams.h"
#include "../Common/MaterialParams.h"
#include "../Common/ShadowParams.h"
#include "../Common/TargetParams.h"


layout(binding=0) uniform TargetParams { TargetData _TargetParams; };
layout(binding=7) uniform CameraParams { CameraData _CameraParams; };

layout(std430, binding = 5) readonly buffer SpotLightParams { LightData _SpotLightParams[]; };
layout(std430, binding = 8) readonly buffer SpotLUT { LightShadowLUTData _SpotLUT[]; };
layout(std430, binding = 9) readonly buffer ShadowParams { ShadowData _ShadowParams[]; };

layout(binding=11) uniform sampler2DArrayShadow SpotShadows;


void PShader()
{	
	const GBuffer gbuffer = UnpackGBuffer(gl_FragCoord.xy);

	if (gbuffer.MaterialID != MAT_ID_GLTF_PBRMetallicRoughness)
	{
		discard;
	}

	const vec2 screenUV = PixelCoordsToScreenUV(gl_FragCoord.xy, _TargetParams.g_targetDims.xy, vec2(0, 0), true);

	const LightShadowLUTData indexLUT = _SpotLUT[0];
	
	const uint lightBufferIdx = indexLUT.g_lightShadowIdx.x;
	const uint shadowBufferIdx = indexLUT.g_lightShadowIdx.y;
	const uint shadowTexIdx = indexLUT.g_lightShadowIdx.z;
			
	const LightData lightData = _SpotLightParams[lightBufferIdx];

	const vec3 worldPos = ScreenUVToWorldPos(screenUV, gbuffer.NonLinearDepth, _CameraParams.g_invViewProjection);
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

	float shadowFactor = 1.f;
	if (shadowBufferIdx != INVALID_SHADOW_IDX)
	{
		const ShadowData shadowData = _ShadowParams[shadowBufferIdx];
		
		const bool shadowEnabled = shadowData.g_shadowParams.x > 0.f;
		if (shadowEnabled)
		{
			const vec2 shadowCamNearFar = shadowData.g_shadowCamNearFarBiasMinMax.xy;
			const vec2 minMaxShadowBias = shadowData.g_shadowCamNearFarBiasMinMax.zw;
			const vec2 lightUVRadiusSize = shadowData.g_shadowParams.zw;
			const float shadowQualityMode = shadowData.g_shadowParams.y;
					
			shadowFactor = Get2DShadowFactor(
				worldPos,
				gbuffer.WorldNormal,
				lightWorldPos,
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
	lightingParams.F0 = gbuffer.MatProp0;

	const float NoL = clamp(dot(gbuffer.WorldNormal, lightWorldDir), 0.f, 1.f);
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

	FragColor = vec4(ComputeLighting(lightingParams), 0.f);
} 