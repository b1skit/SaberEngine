// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "MathConstants.hlsli"
#include "SaberCommon.hlsli"
#include "Transformations.hlsli"
#include "UVUtils.hlsli"


float4 PShader(VertexOut In) : SV_Target
{
	const float sampleDepth = 0.f; // Arbitrary
	const float3 worldPos = GetWorldPos(In.UV0, sampleDepth, CameraParams.g_invViewProjection);
	
	const float3 sampleDir = worldPos - CameraParams.g_cameraWPos.xyz; // The skybox is centered about the camera

	const float2 sphericalUVs = WorldDirToSphericalUV(sampleDir); // Normalizes incoming sampleDir

	float3 skyboxColor = Tex0.Sample(WrapLinearMipMapLinearLinear, sphericalUVs).rgb;
	
	return float4(skyboxColor, 1.f);
}