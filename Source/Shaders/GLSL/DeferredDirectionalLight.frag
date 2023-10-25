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
	const vec3 keylightWorldDir = g_lightWorldPosRadius.xyz;

	// Read from 2D shadow map:
	const float NoL = max(0.0, dot(gbuffer.WorldNormal, keylightWorldDir));
	const vec3 shadowPos = (g_shadowCam_VP * vec4(worldPos, 1.f)).xyz;
	const float shadowFactor = GetShadowFactor(shadowPos, Depth0, NoL);

	LightingParams lightingParams;
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.RemappedRoughness = RemapRoughness(gbuffer.LinearRoughness);
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	lightingParams.WorldPosition = worldPos;
	lightingParams.F0 = gbuffer.MatProp0;
	lightingParams.LightWorldPos = vec3(0.f, 0.f, 0.f); // Directional lights are at infinity
	lightingParams.LightWorldDir = keylightWorldDir;
	lightingParams.LightColor = g_lightColorIntensity.rgb;
	lightingParams.LightIntensity = g_lightColorIntensity.a;
	lightingParams.LightAttenuationFactor = 1.f;
	lightingParams.ShadowFactor = shadowFactor;

	lightingParams.CameraWorldPos = g_cameraWPos.xyz;
	lightingParams.Exposure = g_exposureProperties.x;

	lightingParams.DiffuseScale = g_intensityScale.x;
	lightingParams.SpecularScale = g_intensityScale.y;

	FragColor = vec4(ComputeLighting(lightingParams), 0.f);
} 