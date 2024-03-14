// © 2023 Adam Badke. All rights reserved.
#define SABER_VEC4_OUTPUT
#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "SaberLighting.glsl"
#include "Shadows.glsl"
#include "GBufferCommon.glsl"


// As per Cem Yuksel's nonsingular point light attenuation function:
// http://www.cemyuksel.com/research/pointlightattenuation/
float ComputePointLightAttenuationFactor(vec3 worldPos, vec3 lightPos, float emitterRadius)
{
	const float r2 = emitterRadius * emitterRadius;

	const float lightDistance = length(worldPos - lightPos);
	const float d2 = lightDistance * lightDistance;
	
	const float attenuation = 2.f / (d2 + r2 + (lightDistance * sqrt(d2 + r2)));
	
	return attenuation;
}


void main()
{	
	const vec2 screenUV = PixelCoordsToUV(gl_FragCoord.xy, _LightParams.g_renderTargetResolution.xy, vec2(0, 0), true);

	const GBuffer gbuffer = UnpackGBuffer(screenUV);

	const vec3 worldPos = GetWorldPos(screenUV, gbuffer.NonLinearDepth, _CameraParams.g_invViewProjection);
	const vec3 lightWorldPos = _LightParams.g_lightWorldPosRadius.xyz;

	const vec3 lightWorldDir = normalize(_LightParams.g_lightWorldPosRadius.xyz - worldPos.xyz);

	const float emitterRadius = _LightParams.g_lightWorldPosRadius.w;
	const float attenuationFactor = ComputePointLightAttenuationFactor(worldPos, lightWorldPos, emitterRadius);

	const vec2 shadowCamNearFar = _LightParams.g_shadowCamNearFarBiasMinMax.xy;
	const vec2 minMaxShadowBias = _LightParams.g_shadowCamNearFarBiasMinMax.zw;
	const float cubeFaceDimension = _LightParams.g_shadowMapTexelSize.x; // Assume the cubemap width/height are the same
	const vec2 lightUVRadiusSize = _LightParams.g_shadowParams.zw;
	const float shadowQualityMode = _LightParams.g_shadowParams.y;

	const bool shadowEnabled = _LightParams.g_shadowParams.x > 0.f;
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
	lightingParams.LightColor = _LightParams.g_lightColorIntensity.rgb;
	lightingParams.LightIntensity = _LightParams.g_lightColorIntensity.a;
	lightingParams.LightAttenuationFactor = attenuationFactor;
	lightingParams.ShadowFactor = shadowFactor;
	
	lightingParams.CameraWorldPos = _CameraParams.g_cameraWPos.xyz;
	lightingParams.Exposure = _CameraParams.g_exposureProperties.x;

	lightingParams.DiffuseScale = _LightParams.g_intensityScale.x;
	lightingParams.SpecularScale = _LightParams.g_intensityScale.y;

	FragColor = vec4(ComputeLighting(lightingParams), 0.f);
} 