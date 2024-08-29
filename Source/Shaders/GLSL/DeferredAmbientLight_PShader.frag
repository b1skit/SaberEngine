// © 2023 Adam Badke. All rights reserved.
#version 460
#define SABER_VEC4_OUTPUT
#define READ_GBUFFER

#include "AmbientCommon.glsl"
#include "GBufferCommon.glsl"


float GetSSAO(vec2 screenUV, uvec2 screenPxDims)
{
	return 1.f; // TODO: Implement this
}


void PShader()
{
	const GBuffer gbuffer = UnpackGBuffer(gl_FragCoord.xy);

	// Reconstruct the world position:
	const vec3 worldPos = ScreenUVToWorldPos(In.UV0.xy, gbuffer.NonLinearDepth, _CameraParams.g_invViewProjection);

	AmbientLightingParams lightingParams;

	lightingParams.WorldPosition = worldPos;
	
	lightingParams.V = normalize(_CameraParams.g_cameraWPos.xyz - worldPos.xyz); // point -> camera
	lightingParams.WorldNormal = normalize(gbuffer.WorldNormal);
	
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	
	lightingParams.DielectricSpecular = gbuffer.MatProp0.rgb;
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.RemappedRoughness = RemapRoughness(gbuffer.LinearRoughness);

	lightingParams.FineAO = gbuffer.AO;
	lightingParams.CoarseAO = GetSSAO(In.UV0, uvec2(_AmbientLightParams.g_ssaoTexDims.xy));
	
	FragColor = vec4(ComputeAmbientLighting(lightingParams), 1.f);
}