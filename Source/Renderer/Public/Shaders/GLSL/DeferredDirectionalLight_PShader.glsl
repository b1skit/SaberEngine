#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsli"
#include "Lighting.glsli"
#include "Shadows.glsli"
#include "GBufferCommon.glsli"

#include "../Common/CameraParams.h"
#include "../Common/LightParams.h"
#include "../Common/MaterialParams.h"
#include "../Common/ShadowParams.h"


layout(binding = 7) uniform CameraParams { CameraData _CameraParams; };

layout(std430, binding = 4) readonly buffer DirectionalLightParams { LightData _DirectionalLightParams[]; };
layout(std430, binding = 6) readonly buffer DirectionalLUT { LightShadowLUTData _DirectionalLUT[]; };
layout(std430, binding = 9) readonly buffer ShadowParams { ShadowData _ShadowParams[]; };

layout(binding = 10) uniform sampler2DArrayShadow DirectionalShadows;


void PShader()
{
	const GBuffer gbuffer = UnpackGBuffer(gl_FragCoord.xy);

	if (gbuffer.MaterialID != MAT_ID_GLTF_PBRMetallicRoughness)
	{
		discard;
	}

	const vec2 screenUV = In.UV0.xy; // Directional light is drawn with a fullscreen quad

	const vec3 worldPos = ScreenUVToWorldPos(screenUV, gbuffer.NonLinearDepth, _CameraParams.g_invViewProjection);

	const LightShadowLUTData indexLUT = _DirectionalLUT[0];
	
	const uint lightBufferIdx = indexLUT.g_lightShadowIdx.x;
	const uint shadowBufferIdx = indexLUT.g_lightShadowIdx.y;
	const uint shadowTexIdx = indexLUT.g_lightShadowIdx.z;
			
	const LightData lightData = _DirectionalLightParams[lightBufferIdx];
	
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
	lightingParams.F0 = gbuffer.MatProp0;

	const float NoL = max(0.0, dot(gbuffer.WorldNormal, lightData.g_lightWorldPosRadius.xyz));
	lightingParams.NoL = NoL;

	lightingParams.LightWorldPos = worldPos; // Ensure attenuation = 0
	lightingParams.LightWorldDir = lightData.g_lightWorldPosRadius.xyz;
	lightingParams.LightColor = lightData.g_lightColorIntensity.rgb;
	lightingParams.LightIntensity = lightData.g_lightColorIntensity.a;
	lightingParams.LightAttenuationFactor = 1.f;
	lightingParams.ShadowFactor = shadowFactor;

	lightingParams.CameraWorldPos = _CameraParams.g_cameraWPos.xyz;
	lightingParams.Exposure = _CameraParams.g_exposureProperties.x;

	lightingParams.DiffuseScale = lightData.g_intensityScale.x;
	lightingParams.SpecularScale = lightData.g_intensityScale.y;

	FragColor = vec4(ComputeLighting(lightingParams), 0.f);
} 