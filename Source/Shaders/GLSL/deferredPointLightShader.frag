#define SABER_VEC4_OUTPUT

#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


void main()
{	
	const vec2 screenUV = PixelCoordsToUV(gl_FragCoord.xy, g_renderTargetResolution.xy, vec2(0, 0), true);

	const GBuffer gbuffer = UnpackGBuffer(screenUV);

	const vec3 worldPos = GetWorldPos(screenUV, gbuffer.NonLinearDepth, g_invViewProjection);
	const vec3 lightWorldDir = normalize(g_lightWorldPos - worldPos.xyz);

	// Cube-map shadows:
	const float NoL = max(0.0, dot(gbuffer.WorldNormal, lightWorldDir));
	const vec3 lightToFrag = worldPos - g_lightWorldPos; // Cubemap sampler dir length matters, so can't use -fragToLight
	const float shadowFactor = GetShadowFactor(lightToFrag, CubeMap0, NoL);

	LightingParams lightingParams;
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.Roughness = gbuffer.Roughness;
	lightingParams.Metalness = gbuffer.Metalness;
	lightingParams.AO = gbuffer.AO;
	lightingParams.WorldPosition = worldPos;
	lightingParams.F0 = gbuffer.MatProp0;
	lightingParams.LightWorldPos = g_lightWorldPos;
	lightingParams.LightWorldDir = lightWorldDir;
	lightingParams.LightColor = g_lightColorIntensity;
	lightingParams.ShadowFactor = shadowFactor;
	lightingParams.View = g_view;

	FragColor = vec4(ComputeLighting(lightingParams), 1.f);
} 