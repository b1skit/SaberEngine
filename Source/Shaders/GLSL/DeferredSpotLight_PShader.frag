// © 2023 Adam Badke. All rights reserved.
#define SABER_VEC4_OUTPUT
#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "Lighting.glsl"
#include "Shadows.glsl"
#include "GBufferCommon.glsl"


// As per equation 64, section 5.2.2.2 of "Physically Based Rendering in Filament"
// https://google.github.io/filament/Filament.md.html#lighting/directlighting/punctuallights
float GetSpotlightAngleAttenuation(
	vec3 toLight, 
	vec3 lightWorldForwardDir, 
	float innerConeAngle, 
	float outerConeAngle, 
	float cosOuterAngle, 
	float scaleTerm,
	float offsetTerm)
{
	const float cd = dot(normalize(-toLight), lightWorldForwardDir);
	
	float attenuation = clamp(cd * scaleTerm + offsetTerm, 0.f, 1.f);
	
	return attenuation * attenuation; // Smooths the resulting transition
}


void main()
{	
	const GBuffer gbuffer = UnpackGBuffer(gl_FragCoord.xy);

	const vec2 screenUV = PixelCoordsToScreenUV(gl_FragCoord.xy, _TargetParams.g_targetDims.xy, vec2(0, 0), true);

	const uint lightParamsIdx = _LightIndexParams.g_lightIndex.x;
	const LightData lightData = _SpotLightParams[lightParamsIdx];

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

	const vec2 shadowCamNearFar = lightData.g_shadowCamNearFarBiasMinMax.xy;
	const vec2 minMaxShadowBias = lightData.g_shadowCamNearFarBiasMinMax.zw;
	const vec2 lightUVRadiusSize = lightData.g_shadowParams.zw;
	const float shadowQualityMode = lightData.g_shadowParams.y;

	const bool shadowEnabled = lightData.g_shadowParams.x > 0.f;
		const float shadowFactor = shadowEnabled ?
		Get2DShadowFactor(
			worldPos,
			gbuffer.WorldNormal,
			lightWorldPos,
			lightData.g_shadowCam_VP,
			shadowCamNearFar,
			minMaxShadowBias,
			shadowQualityMode,
			lightUVRadiusSize,
			lightData.g_shadowMapTexelSize) : 1.f;

	const float NoL = clamp(dot(gbuffer.WorldNormal, lightWorldDir), 0.f, 1.f);

	LightingParams lightingParams;
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.RemappedRoughness = RemapRoughness(gbuffer.LinearRoughness);
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	lightingParams.WorldPosition = worldPos;
	lightingParams.F0 = gbuffer.MatProp0;
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