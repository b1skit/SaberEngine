#define SABER_VEC4_OUTPUT
#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "Lighting.glsl"
#include "Shadows.glsl"
#include "GBufferCommon.glsl"


void main()
{
	const GBuffer gbuffer = UnpackGBuffer(gl_FragCoord.xy);

	const vec2 screenUV = In.uv0.xy; // Directional light is drawn with a fullscreen quad

	const vec3 worldPos = ScreenUVToWorldPos(screenUV, gbuffer.NonLinearDepth, _CameraParams.g_invViewProjection);

	const vec2 shadowCamNearFar = _LightParams.g_shadowCamNearFarBiasMinMax.xy;
	const vec2 minMaxShadowBias = _LightParams.g_shadowCamNearFarBiasMinMax.zw;
	const vec2 invShadowMapWidthHeight = _LightParams.g_shadowMapTexelSize.zw;
	const vec2 lightUVRadiusSize = _LightParams.g_shadowParams.zw;
	const float shadowQualityMode = _LightParams.g_shadowParams.y;

	const bool shadowEnabled = _LightParams.g_shadowParams.x > 0.f;
	const float shadowFactor = shadowEnabled ?
		Get2DShadowFactor(
			worldPos,
			gbuffer.WorldNormal,
			_LightParams.g_lightWorldPosRadius.xyz,
			_LightParams.g_shadowCam_VP,
			shadowCamNearFar,
			minMaxShadowBias,
			shadowQualityMode,
			lightUVRadiusSize,
			invShadowMapWidthHeight) : 1.f;

	const float NoL = max(0.0, dot(gbuffer.WorldNormal, _LightParams.g_lightWorldPosRadius.xyz));

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
	lightingParams.LightWorldDir = _LightParams.g_lightWorldPosRadius.xyz;
	lightingParams.LightColor = _LightParams.g_lightColorIntensity.rgb;
	lightingParams.LightIntensity = _LightParams.g_lightColorIntensity.a;
	lightingParams.LightAttenuationFactor = 1.f;
	lightingParams.ShadowFactor = shadowFactor;

	lightingParams.CameraWorldPos = _CameraParams.g_cameraWPos.xyz;
	lightingParams.Exposure = _CameraParams.g_exposureProperties.x;

	lightingParams.DiffuseScale = _LightParams.g_intensityScale.x;
	lightingParams.SpecularScale = _LightParams.g_intensityScale.y;

	FragColor = vec4(ComputeLighting(lightingParams), 0.f);
} 