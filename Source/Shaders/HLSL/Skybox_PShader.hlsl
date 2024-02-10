// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "MathConstants.hlsli"
#include "SaberCommon.hlsli"
#include "Transformations.hlsli"
#include "UVUtils.hlsli"


struct SkyboxParamsCB
{
	float4 g_backgroundColorIsEnabled; // .rgb = background color override, .a = enabled/disabled (1.f/0.f)
};
ConstantBuffer<SkyboxParamsCB> SkyboxParams;


float4 PShader(VertexOut In) : SV_Target
{
	// Debug: Override the skybox with a flat color
	if (SkyboxParams.g_backgroundColorIsEnabled.a == 1.f)
	{
		return SkyboxParams.g_backgroundColorIsEnabled;
	}
	
	const float sampleDepth = 0.f; // Arbitrary
	const float3 worldPos = GetWorldPos(In.UV0, sampleDepth, CameraParams.g_invViewProjection);
	
	const float3 sampleDir = worldPos - CameraParams.g_cameraWPos.xyz; // The skybox is centered about the camera

	const float2 sphericalUVs = WorldDirToSphericalUV(sampleDir); // Normalizes incoming sampleDir

	// Manually sample mip 0, as otherwise we get a nasty seam due to the discontinuity in our UV derivatives (i.e.
	// atan2 when computing the spherical UVs)
	float3 skyboxColor = Tex0.SampleLevel(WrapMinMagLinearMipPoint, sphericalUVs, 0).rgb;
	
	return float4(skyboxColor, 1.f);
}