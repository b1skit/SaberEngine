#define SABER_VEC4_OUTPUT
#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "SaberLighting.glsl"
#include "Shadows.glsl"
#include "GBufferCommon.glsl"


void main()
{
	const vec2 screenUV = vOut.uv0.xy; // Directional light is drawn with a fullscreen quad

	const GBuffer gbuffer = UnpackGBuffer(screenUV);

	const vec3 worldPos = GetWorldPos(screenUV, gbuffer.NonLinearDepth, g_invViewProjection);

	const vec2 shadowCamNearFar = g_shadowCamNearFarBiasMinMax.xy;
	const vec2 minMaxShadowBias = g_shadowCamNearFarBiasMinMax.zw;
	const vec2 invShadowMapWidthHeight = g_shadowMapTexelSize.zw;
	const vec2 lightUVRadiusSize = g_shadowParams.zw;
	const float shadowQualityMode = g_shadowParams.y;

	const bool shadowEnabled = g_shadowParams.x > 0.f;
	const float shadowFactor = shadowEnabled ?
		Get2DShadowFactor(
			worldPos,
			gbuffer.WorldNormal,
			g_lightWorldPosRadius.xyz,
			g_shadowCam_VP,
			shadowCamNearFar,
			minMaxShadowBias,
			shadowQualityMode,
			lightUVRadiusSize,
			invShadowMapWidthHeight) : 1.f;

	const float NoL = max(0.0, dot(gbuffer.WorldNormal, g_lightWorldPosRadius.xyz));

	LightingParams lightingParams;
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.RemappedRoughness = RemapRoughness(gbuffer.LinearRoughness);
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	lightingParams.WorldPosition = worldPos;
	lightingParams.F0 = gbuffer.MatProp0;

	lightingParams.NoL = NoL;

	lightingParams.LightWorldPos = worldPos; // Ensure attenuation = 0
	lightingParams.LightWorldDir = g_lightWorldPosRadius.xyz;
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