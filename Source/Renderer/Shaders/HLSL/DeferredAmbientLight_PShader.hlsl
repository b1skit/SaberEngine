// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#include "AmbientCommon.hlsli"
#include "GBufferCommon.hlsli"

#include "../Common/CameraParams.h"
#include "../Common/MaterialParams.h"

ConstantBuffer<CameraData> CameraParams : register(space1);

Texture2D<float> AOTex;


float GetSSAO(float2 screenUV, uint2 screenPxDims)
{
	const uint3 coords = uint3(screenUV * screenPxDims, 0);
	
	return AOTex.Load(coords).r;
}


float4 PShader(VertexOut In) : SV_Target
{
	const GBuffer gbuffer = UnpackGBuffer(In.Position.xy);
	
	if (gbuffer.MaterialID != MAT_ID_GLTF_PBRMetallicRoughness)
	{
		return float4(0.f, 0.f, 0.f, 0.f);
	}
	
	// Reconstruct the world position:
	const float3 worldPos = ScreenUVToWorldPos(In.UV0, gbuffer.NonLinearDepth, CameraParams.g_invViewProjection);
	
	AmbientLightingParams lightingParams;
	
	lightingParams.WorldPosition = worldPos;
	
	lightingParams.V = normalize(CameraParams.g_cameraWPos.xyz - worldPos); // point -> camera
	lightingParams.WorldNormal = normalize(gbuffer.WorldNormal);
	
	lightingParams.LinearAlbedo = gbuffer.LinearAlbedo;
	
	lightingParams.DielectricSpecular = gbuffer.MatProp0.rgb;
	lightingParams.LinearMetalness = gbuffer.LinearMetalness;
	
	lightingParams.LinearRoughness = gbuffer.LinearRoughness;
	lightingParams.RemappedRoughness = RemapRoughness(gbuffer.LinearRoughness);

	lightingParams.FineAO = gbuffer.AO;
	lightingParams.CoarseAO = GetSSAO(In.UV0, AmbientLightParams.g_AOTexDims.xy);
	
	return float4(ComputeAmbientLighting(lightingParams), 1.f);
}