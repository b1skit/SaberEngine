// © 2023 Adam Badke. All rights reserved.
#define SABER_VEC4_OUTPUT
#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
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
	const vec2 screenUV = PixelCoordsToUV(gl_FragCoord.xy, g_renderTargetResolution.xy, vec2(0, 0), true);

	const GBuffer gbuffer = UnpackGBuffer(screenUV);

	const vec3 worldPos = GetWorldPos(screenUV, gbuffer.NonLinearDepth, g_invViewProjection);
	const vec3 lightWorldPos = g_lightWorldPosRadius.xyz;

	const vec3 lightWorldDir = normalize(g_lightWorldPosRadius.xyz - worldPos.xyz);

	const float emitterRadius = g_lightWorldPosRadius.w;
	const float attenuationFactor = ComputePointLightAttenuationFactor(worldPos, lightWorldPos, emitterRadius);

	// Cube-map shadows:
	const float NoL = max(0.0, dot(gbuffer.WorldNormal, lightWorldDir));
	const vec3 lightToFrag = worldPos - g_lightWorldPosRadius.xyz; // Cubemap sampler dir length matters, so can't use -fragToLight

	const bool shadowEnabled = g_intensityScaleShadowed.z > 0.f;
	const float shadowFactor = shadowEnabled ? GetShadowFactor(lightToFrag, CubeDepth, NoL) : 1.f;

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
	lightingParams.LightColor = g_lightColorIntensity.rgb;
	lightingParams.LightIntensity = g_lightColorIntensity.a;
	lightingParams.LightAttenuationFactor = attenuationFactor;
	lightingParams.ShadowFactor = shadowFactor;
	
	lightingParams.CameraWorldPos = g_cameraWPos.xyz;
	lightingParams.Exposure = g_exposureProperties.x;

	lightingParams.DiffuseScale = g_intensityScaleShadowed.x;
	lightingParams.SpecularScale = g_intensityScaleShadowed.y;

	FragColor = vec4(ComputeLighting(lightingParams), 0.f);
} 