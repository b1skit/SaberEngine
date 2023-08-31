#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


void main()
{
	const vec2 screenUV = vOut.uv0.xy; // Directional light is drawn with a fullscreen quad

	const GBuffer gbuffer = UnpackGBuffer(screenUV);

	const vec3 worldPos = GetWorldPos(screenUV, gbuffer.NonLinearDepth, g_invViewProjection);

	// Directional light direction is packed into the light position
	const vec3 keylightWorldDir = g_lightWorldPos;

	// Read from 2D shadow map:
	const float NoL = max(0.0, dot(gbuffer.WorldNormal, keylightWorldDir));
	const vec3 shadowPos = (g_shadowCam_VP * vec4(worldPos, 1.f)).xyz;
	const float shadowFactor = GetShadowFactor(shadowPos, Depth0, NoL);

	LightingParams lightingParams;
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.Roughness = gbuffer.Roughness;
	lightingParams.Metalness = gbuffer.Metalness;
	lightingParams.AO = gbuffer.AO;
	lightingParams.WorldPosition = worldPos;
	lightingParams.F0 = gbuffer.MatProp0;
	lightingParams.LightWorldPos = worldPos; // Ensure attenuation = 0 for directional lights
	lightingParams.LightWorldDir = keylightWorldDir;
	lightingParams.LightColor = g_lightColorIntensity;
	lightingParams.ShadowFactor = shadowFactor;
	lightingParams.View = g_view;

	FragColor = vec4(ComputeLighting(lightingParams), 1.f);
} 