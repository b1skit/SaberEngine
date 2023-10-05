// © 2023 Adam Badke. All rights reserved.
#define SABER_VEC4_OUTPUT
#define READ_GBUFFER
#define VOUT_WORLD_POS

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


void main()
{	
	const vec2 screenUV = PixelCoordsToUV(gl_FragCoord.xy, g_renderTargetResolution.xy, vec2(0, 0), true);

	const GBuffer gbuffer = UnpackGBuffer(screenUV);

	const vec3 worldPos = GetWorldPos(screenUV, gbuffer.NonLinearDepth, g_invViewProjection);
	const vec3 lightWorldDir = normalize(g_lightWorldPos.xyz - worldPos.xyz);

	// Cube-map shadows:
	const float NoL = max(0.0, dot(gbuffer.WorldNormal, lightWorldDir));
	const vec3 lightToFrag = worldPos - g_lightWorldPos.xyz; // Cubemap sampler dir length matters, so can't use -fragToLight
	const float shadowFactor = GetShadowFactor(lightToFrag, CubeMap0, NoL);

	LightingParams lightingParams;
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	lightingParams.AO = gbuffer.AO;
	lightingParams.WorldPosition = worldPos;
	lightingParams.F0 = gbuffer.MatProp0;
	lightingParams.LightWorldPos = g_lightWorldPos.xyz;
	lightingParams.LightWorldDir = lightWorldDir;
	lightingParams.LightColor = g_lightColorIntensity.rgb;
	lightingParams.ShadowFactor = shadowFactor;
	lightingParams.View = g_view;

	FragColor = vec4(ComputeLighting(lightingParams), 1.f);
} 