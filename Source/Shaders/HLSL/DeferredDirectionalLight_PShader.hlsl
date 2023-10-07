// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "GBufferCommon.hlsli"
#include "Lighting.hlsli"
#include "MathConstants.hlsli"
#include "SaberCommon.hlsli"
#include "Transformations.hlsli"


struct LightParamsCB
{
	float4 g_lightColorIntensity; // .rgb = hue, .a = intensity
	float4 g_lightWorldPos; // Directional lights: Normalized, world-space point to source dir (ie. parallel)
	float4 g_shadowMapTexelSize; // .xyzw = width, height, 1/width, 1/height
	float4 g_shadowCamNearFarBiasMinMax; // .xy = shadow cam near/far, .zw = min, max shadow bias

	float4x4 g_shadowCam_VP;
	
	float4 g_renderTargetResolution;
	float4 g_intensityScale; // .xy = diffuse/specular intensity scale, .zw = unused
};
ConstantBuffer<LightParamsCB> LightParams;


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.UV0);
	
	const float4 worldPos = float4(GetWorldPos(In.UV0, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection), 1.f);
	
	LightingParams lightingParams;
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	lightingParams.WorldNormal = gbuffer.WorldNormal;
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.RemappedRoughness = RemapRoughness(gbuffer.LinearRoughness);
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	lightingParams.WorldPosition = worldPos.xyz;
	lightingParams.F0 = gbuffer.MatProp0.rgb;
	lightingParams.LightWorldPos = worldPos.xyz; // Ensure attenuation = 0
	lightingParams.LightWorldDir = LightParams.g_lightWorldPos.xyz; 
	lightingParams.LightColor = LightParams.g_lightColorIntensity.rgb;
	lightingParams.LightIntensity = LightParams.g_lightColorIntensity.a;
	lightingParams.ShadowFactor = 1.f; // TODO: Compute this
	lightingParams.CameraWorldPos = CameraParams.g_cameraWPos;
	lightingParams.DiffuseScale = LightParams.g_intensityScale.x;
	lightingParams.SpecularScale = LightParams.g_intensityScale.y;
	
	return float4(ComputeLighting(lightingParams), 1.f);
}