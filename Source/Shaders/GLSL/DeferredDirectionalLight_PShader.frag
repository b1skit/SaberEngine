#define SABER_VEC4_OUTPUT
#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "Lighting.glsl"
#include "Shadows.glsl"
#include "GBufferCommon.glsl"


void PShader()
{
	const GBuffer gbuffer = UnpackGBuffer(gl_FragCoord.xy);

	const vec2 screenUV = In.UV0.xy; // Directional light is drawn with a fullscreen quad

	const vec3 worldPos = ScreenUVToWorldPos(screenUV, gbuffer.NonLinearDepth, _CameraParams.g_invViewProjection);

	const uint lightParamsIdx = _LightIndexParams.g_lightIndex.x;
	const uint shadowIdx = _LightIndexParams.g_lightIndex.y;

	const LightData lightData = _DirectionalLightParams[lightParamsIdx];

	const vec2 shadowCamNearFar = lightData.g_shadowCamNearFarBiasMinMax.xy;
	const vec2 minMaxShadowBias = lightData.g_shadowCamNearFarBiasMinMax.zw;
	const vec2 lightUVRadiusSize = lightData.g_shadowParams.zw;
	const float shadowQualityMode = lightData.g_shadowParams.y;

	const bool shadowEnabled = lightData.g_shadowParams.x > 0.f;
	const float shadowFactor = shadowEnabled ?
		Get2DShadowFactor(
			worldPos,
			gbuffer.WorldNormal,
			lightData.g_lightWorldPosRadius.xyz,
			lightData.g_shadowCam_VP,
			shadowCamNearFar,
			minMaxShadowBias,
			shadowQualityMode,
			lightUVRadiusSize,
			lightData.g_shadowMapTexelSize,
			DirectionalShadows,
			shadowIdx) : 1.f;

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