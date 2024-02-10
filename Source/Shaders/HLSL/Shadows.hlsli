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
			depthSum += (biasedPointDepth < Depth0.SampleLevel(ClampMinMagLinearMipPoint, shadowmapUVs, 0).r) ? 1.f : 0.f;

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
	float light = CubeMap0.SampleLevel(WrapMinMagLinearMipPoint, cubeSampleDir, 0).r > nonLinearPointDepth ? 1.f : 0.f;

	// Get 4 extra samples at diagonal offsets:
	cubeSampleDir.xy -= oxy;
	cubeSampleDir.yz -= oyz;

	light += CubeMap0.SampleLevel(WrapMinMagLinearMipPoint, clamp(cubeSampleDir, -limit, limit), 0).r > nonLinearPointDepth ? 1.f : 0.f;
	cubeSampleDir.xy += oxy * 2.f;

	light += CubeMap0.SampleLevel(WrapMinMagLinearMipPoint, clamp(cubeSampleDir, -limit, limit), 0).r > nonLinearPointDepth ? 1.f : 0.f;
	cubeSampleDir.yz += oyz * 2.f;

	light += CubeMap0.SampleLevel(WrapMinMagLinearMipPoint, clamp(cubeSampleDir, -limit, limit), 0).r > nonLinearPointDepth ? 1.f : 0.f;
	cubeSampleDir.xy -= oxy * 2.f;

	light += CubeMap0.SampleLevel(WrapMinMagLinearMipPoint, clamp(cubeSampleDir, -limit, limit), 0).r > nonLinearPointDepth ? 1.f : 0.f;

	return (light * 0.2); // Return the average of our 5 samples
}








//#define PCSS_STUFF
#ifdef PCSS_STUFF






// PRESET is defined by the app when (re)loading the fx
#define PRESET 1

#if PRESET == 0

#define USE_POISSON
#define SEARCH_POISSON_COUNT 25
#define SEARCH_POISSON Poisson25
#define PCF_POISSON_COUNT 25
#define PCF_POISSON Poisson25

#elif PRESET == 1

#define USE_POISSON
#define SEARCH_POISSON_COUNT 32
#define SEARCH_POISSON Poisson32
#define PCF_POISSON_COUNT 64
#define PCF_POISSON Poisson64

#else

#define BLOCKER_SEARCH_STEP_COUNT 3
#define PCF_FILTER_STEP_COUNT 7

#endif

Texture2D<float> tDepthMap;
float2 g_LightRadiusUV;
float g_LightZNear;
float g_LightZFar;

row_major float4x4 mWorldViewProj;
row_major float4x4 mLightView;
row_major float4x4 mLightViewProj;
row_major float4x4 mLightViewProjClip2Tex;

// Using similar triangles from the surface point to the area light
float2 SearchRegionRadiusUV(float zWorld)
{
	return g_LightRadiusUV * (zWorld - g_LightZNear) / zWorld;
}

// Using similar triangles between the area light, the blocking plane and the surface point
float2 PenumbraRadiusUV(float zReceiver, float zBlocker)
{
	return g_LightRadiusUV * (zReceiver - zBlocker) / zBlocker;
}

// Project UV size to the near plane of the light
float2 ProjectToLightUV(float2 sizeUV, float zWorld)
{
	return sizeUV * g_LightZNear / zWorld;
}

// Derivatives of light-space depth with respect to texture coordinates
float2 DepthGradient(float2 uv, float z)
{
	float2 dz_duv = 0;

	float3 duvdist_dx = ddx(float3(uv, z));
	float3 duvdist_dy = ddy(float3(uv, z));

	dz_duv.x = duvdist_dy.y * duvdist_dx.z;
	dz_duv.x -= duvdist_dx.y * duvdist_dy.z;
	
	dz_duv.y = duvdist_dx.x * duvdist_dy.z;
	dz_duv.y -= duvdist_dy.x * duvdist_dx.z;

	float det = (duvdist_dx.x * duvdist_dy.y) - (duvdist_dx.y * duvdist_dy.x);
	dz_duv /= det;

	return dz_duv;
}

float BiasedZ(float z0, float2 dz_duv, float2 offset)
{
	return z0 + dot(dz_duv, offset);
}

float ZClipToZEye(float zClip)
{
	return g_LightZFar * g_LightZNear / (g_LightZFar - zClip * (g_LightZFar - g_LightZNear));
}

// Returns average blocker depth in the search region, as well as the number of found blockers.
// Blockers are defined as shadow-map samples between the surface point and the light.
void FindBlocker(out float avgBlockerDepth,
				out float numBlockers,
				Texture2D<float> tDepthMap,
				float2 uv,
				float z0,
				float2 dz_duv,
				float2 searchRegionRadiusUV)
{
	float blockerSum = 0;
	numBlockers = 0;

#ifdef USE_POISSON
	for ( int i = 0; i < SEARCH_POISSON_COUNT; ++i )
	{
		float2 offset = SEARCH_POISSON[i] * searchRegionRadiusUV;
		float shadowMapDepth = tDepthMap.SampleLevel(PointSampler, uv + offset, 0);
		float z = BiasedZ(z0, dz_duv, offset);
		if ( shadowMapDepth < z )
		{
			blockerSum += shadowMapDepth;
			numBlockers++;
		}
	}
#else
	float2 stepUV = searchRegionRadiusUV / BLOCKER_SEARCH_STEP_COUNT;
	for (float x = -BLOCKER_SEARCH_STEP_COUNT; x <= BLOCKER_SEARCH_STEP_COUNT; ++x)
		for (float y = -BLOCKER_SEARCH_STEP_COUNT; y <= BLOCKER_SEARCH_STEP_COUNT; ++y)
		{
			float2 offset = float2(x, y) * stepUV;
			float shadowMapDepth = tDepthMap.SampleLevel(PointSampler, uv + offset, 0);
			float z = BiasedZ(z0, dz_duv, offset);
			if (shadowMapDepth < z)
			{
				blockerSum += shadowMapDepth;
				numBlockers++;
			}
		}
#endif
	avgBlockerDepth = blockerSum / numBlockers;
}

// Performs PCF filtering on the shadow map using multiple taps in the filter region.
float PCF_Filter(float2 uv, float z0, float2 dz_duv, float2 filterRadiusUV)
{
	float sum = 0;
	
#ifdef USE_POISSON
	for ( int i = 0; i < PCF_POISSON_COUNT; ++i )
	{
		float2 offset = PCF_POISSON[i] * filterRadiusUV;
		float z = BiasedZ(z0, dz_duv, offset);
		sum += tDepthMap.SampleCmpLevelZero(PCF_Sampler, uv + offset, z);
	}
	return sum / PCF_POISSON_COUNT;
#else
	float2 stepUV = filterRadiusUV / PCF_FILTER_STEP_COUNT;
	for (float x = -PCF_FILTER_STEP_COUNT; x <= PCF_FILTER_STEP_COUNT; ++x)
		for (float y = -PCF_FILTER_STEP_COUNT; y <= PCF_FILTER_STEP_COUNT; ++y)
		{
			float2 offset = float2(x, y) * stepUV;
			float z = BiasedZ(z0, dz_duv, offset);
			sum += tDepthMap.SampleCmpLevelZero(PCF_Sampler, uv + offset, z);
		}
	float numSamples = (PCF_FILTER_STEP_COUNT * 2 + 1);
	return sum / (numSamples * numSamples);
#endif
}

float PCSS_Shadow(float2 uv, float z, float2 dz_duv, float zEye)
{
	// ------------------------
	// STEP 1: blocker search
	// ------------------------
	float avgBlockerDepth = 0;
	float numBlockers = 0;
	float2 searchRegionRadiusUV = SearchRegionRadiusUV(zEye);
	FindBlocker(avgBlockerDepth, numBlockers, tDepthMap, uv, z, dz_duv, searchRegionRadiusUV);

	// Early out if no blocker found
	if (numBlockers == 0)
		return 1.0;

	// ------------------------
	// STEP 2: penumbra size
	// ------------------------
	float avgBlockerDepthWorld = ZClipToZEye(avgBlockerDepth);
	float2 penumbraRadiusUV = PenumbraRadiusUV(zEye, avgBlockerDepthWorld);
	float2 filterRadiusUV = ProjectToLightUV(penumbraRadiusUV, zEye);
	
	// ------------------------
	// STEP 3: filtering
	// ------------------------
	return PCF_Filter(uv, z, dz_duv, filterRadiusUV);
}

float PCF_Shadow(float2 uv, float z, float2 dz_duv, float zEye)
{
	// Do a blocker search to enable early out
	float avgBlockerDepth = 0;
	float numBlockers = 0;
	float2 searchRegionRadiusUV = SearchRegionRadiusUV(zEye);
	FindBlocker(avgBlockerDepth, numBlockers, tDepthMap, uv, z, dz_duv, searchRegionRadiusUV);
	if (numBlockers == 0)
		return 1.0;

	float2 filterRadiusUV = 0.1 * g_LightRadiusUV;
	return PCF_Filter(uv, z, dz_duv, filterRadiusUV);
}

#endif // PCSS_STUFF









#endif