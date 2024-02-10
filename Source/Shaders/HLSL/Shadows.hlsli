// © 2023 Adam Badke. All rights reserved.
#ifndef SHADOWS_COMMON
#define SHADOWS_COMMON

#include "Transformations.hlsli"
#include "UVUtils.hlsli"


float GetSlopeScaleBias(float NoL, float2 minMaxShadowBias)
{
	return max(minMaxShadowBias.x, minMaxShadowBias.y * (1.f - NoL));
}


float Get2DShadowMapFactor(
	float4 worldPos, float4x4 shadowCamVP, float NoL, float2 minMaxShadowBias, float2 invShadowMapWidthHeight)
{
	// Transform our worldPos to shadow map projection space:
	float4 shadowProjPos = mul(shadowCamVP, worldPos);
	shadowProjPos /= shadowProjPos.w; // Not necessary for orthogonal matrices
	
	const float biasedPointDepth = shadowProjPos.z - GetSlopeScaleBias(NoL, minMaxShadowBias);
	
	// Shadow map UVs:
	float2 shadowmapUVs = (shadowProjPos.xy + 1.f) * 0.5f;
	shadowmapUVs.y = 1.f - shadowmapUVs.y; // Clip space Y+ is up, UV Y+ is down, so we flip Y here
	
	// Compute a block of samples around our fragment, starting at the top-left. Note: MUST be a power of two.
	// TODO: Compute this on C++ side and allow for uploading of arbitrary samples (eg. odd, even)
	static const int gridSize = 4;

	const float offsetMultiplier = (float(gridSize) / 2.f) - 0.5;

	shadowmapUVs.x -= offsetMultiplier * invShadowMapWidthHeight.x;
	shadowmapUVs.y += offsetMultiplier * invShadowMapWidthHeight.y;

	float depthSum = 0;
	for (int row = 0; row < gridSize; row++)
	{
		for (int col = 0; col < gridSize; col++)
		{
			depthSum += Depth0.SampleCmpLevelZero(BorderCmpMinMagLinearMipPoint, shadowmapUVs, biasedPointDepth);
			
			shadowmapUVs.x += LightParams.g_shadowMapTexelSize.z;
		}

		shadowmapUVs.x -= gridSize * LightParams.g_shadowMapTexelSize.z;
		shadowmapUVs.y -= LightParams.g_shadowMapTexelSize.w;
	}

	depthSum /= (gridSize * gridSize);
	
	return depthSum;
}


// Compute a soft shadow factor from a cube map
// Based on Lengyel's Foundations of Game Engine Development Volume 2: Rendering, p164, listing 8.8
float GetCubeShadowMapFactor(
	float3 worldPos, float3 lightWorldPos, float NoL, float2 shadowCamNearFar, float2 minMaxShadowBias, float cubeFaceDim)
{
	const float3 lightToPoint = worldPos - lightWorldPos;
	float3 cubeSampleDir = WorldToCubeSampleDir(lightToPoint);
	
	// Calculate non-linear, projected depth buffer depth from the light-to-fragment direction. The eye depth w.r.t
	// our cubemap view is the value of the largest component of this direction. Also apply a slope-scale bias.
	const float3 absSampleDir = abs(cubeSampleDir);
	const float maxXY = max(absSampleDir.x, absSampleDir.y);
	const float eyeDepth = max(maxXY, absSampleDir.z);
	const float biasedEyeDepth = eyeDepth - GetSlopeScaleBias(NoL, minMaxShadowBias);
	
	const float nonLinearPointDepth =
		ConvertLinearDepthToNonLinear(shadowCamNearFar.x, shadowCamNearFar.y, biasedEyeDepth);
	
	// Compute a sample offset for PCF shadow samples:
	const float sampleOffset = 2.f / cubeFaceDim;
	
	// Calculate offset vectors:
	const float offset = sampleOffset * eyeDepth;
	const float dxy = (maxXY > absSampleDir.z) ? offset : 0.f;
	const float dx = (absSampleDir.x > absSampleDir.y) ? dxy : 0.f;
	const float2 oxy = float2(offset - dx, dx);
	const float2 oyz = float2(offset - dxy, dxy);

	float3 limit = float3(eyeDepth, eyeDepth, eyeDepth);
	const float bias = 1.f / 1024.f; // Epsilon = 1/1024.

	limit.xy -= oxy * bias;
	limit.yz -= oyz * bias;

	// Get the center sample:
	float light = CubeDepth.SampleCmpLevelZero(
		WrapCmpMinMagLinearMipPoint, 
		cubeSampleDir, 
		nonLinearPointDepth);
	
	// Get 4 extra samples at diagonal offsets:
	cubeSampleDir.xy -= oxy;
	cubeSampleDir.yz -= oyz;

	light += CubeDepth.SampleCmpLevelZero(
		WrapCmpMinMagLinearMipPoint,
		clamp(cubeSampleDir, -limit, limit), 
		nonLinearPointDepth);
	cubeSampleDir.xy += oxy * 2.f;

	light += CubeDepth.SampleCmpLevelZero(
		WrapCmpMinMagLinearMipPoint,
		clamp(cubeSampleDir, -limit, limit), 
		nonLinearPointDepth);
	cubeSampleDir.yz += oyz * 2.f;

	light += CubeDepth.SampleCmpLevelZero(
		WrapCmpMinMagLinearMipPoint,
		clamp(cubeSampleDir, -limit, limit), 
		nonLinearPointDepth);
	cubeSampleDir.xy -= oxy * 2.f;

	light += CubeDepth.SampleCmpLevelZero(
		WrapCmpMinMagLinearMipPoint,
		clamp(cubeSampleDir, -limit, limit),
		nonLinearPointDepth);

	return (light * 0.2); // Return the average of our 5 samples
}


#endif