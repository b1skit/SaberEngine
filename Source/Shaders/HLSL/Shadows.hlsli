// © 2023 Adam Badke. All rights reserved.
#ifndef SHADOWS_COMMON
#define SHADOWS_COMMON

#include "Lighting.hlsli"
#include "Transformations.hlsli"
#include "UVUtils.hlsli"

#include "../Common/LightParams.h"


ConstantBuffer<PoissonSampleParamsData> PoissonSampleParams;

Texture2DArray<float> DirectionalShadows;
Texture2DArray<float> SpotShadows;
TextureCubeArray<float> PointShadows;


void Float2ToFloat4SampleIndex(uint sampleIdx, out uint baseIdxOut, out uint offsetOut)
{
	baseIdxOut = sampleIdx / 2;
	offsetOut = (sampleIdx % 2) * 2;
}

float2 Get2DPoissonSample64(uint sampleIdx)
{
	uint baseIdx, offset;
	Float2ToFloat4SampleIndex(sampleIdx, baseIdx, offset);
	
	return float2(
		PoissonSampleParams.g_poissonSamples64[baseIdx][offset],
		PoissonSampleParams.g_poissonSamples64[baseIdx][offset + 1]);
}
float2 Get2DPoissonSample32(uint sampleIdx)
{
	uint baseIdx, offset;
	Float2ToFloat4SampleIndex(sampleIdx, baseIdx, offset);
	
	return float2(
		PoissonSampleParams.g_poissonSamples32[baseIdx][offset],
		PoissonSampleParams.g_poissonSamples32[baseIdx][offset + 1]);
}
float2 Get2DPoissonSample25(uint sampleIdx)
{
	uint baseIdx, offset;
	Float2ToFloat4SampleIndex(sampleIdx, baseIdx, offset);
	
	return float2(
		PoissonSampleParams.g_poissonSamples25[baseIdx][offset],
		PoissonSampleParams.g_poissonSamples25[baseIdx][offset + 1]);
}

float2 GetPoissonBlockerSample(uint sampleIdx, uint qualityMode)
{
	switch (qualityMode)
	{
		case 2: // PCSS high
		{
			return Get2DPoissonSample32(sampleIdx);
		}
		break;
		case 1: // PCSS low
		case 0: // case 0 and default should never happen...
		default:
		{
			return Get2DPoissonSample25(sampleIdx);
		}
	}
}

float2 GetPoissonPenumbraSample(uint sampleIdx, uint qualityMode)
{
	switch (qualityMode)
	{
		case 2: // PCSS high
		{
			return Get2DPoissonSample64(sampleIdx);
		}
		break;
		case 1: // PCSS low
		case 0: // case 0 and default should never happen...
		default:
		{
			return Get2DPoissonSample25(sampleIdx);
		}
	}
}


// Using similar triangles from the surface point to the area light
float2 SearchRegionRadiusUV(float2 lightUVRadiusSize, float shadowCamNear, float eyeDepth)
{	
	return lightUVRadiusSize * (eyeDepth - shadowCamNear) / eyeDepth;
	// NOTE: Shadow frustums defined with near/far planes of different signs (which is possible with orthographic 
	// projections) will result in a division by zero at some point along the Z axis in view space: Don't allow this!
}


// Compute the estimated penumbra radius via similar triangles between the light, the blocking plane & the surface point
float2 PenumbraRadiusUV(float2 lightUVRadiusSize, float eyeDepth, float avgBlockerEyeDepth)
{
	return lightUVRadiusSize * (eyeDepth - avgBlockerEyeDepth) / avgBlockerEyeDepth;
}


// Project UV size to the near plane of the light
float2 ProjectToLightUV(float shadowCamNear, float2 sizeUV, float eyeDepth)
{
	return sizeUV * shadowCamNear / eyeDepth;
}


// Find the number of samples between the shading point & light (blockers) & their average depth within the UV region
void FindBlocker(
	out float avgBlockerDepth,
	out float numBlockers,
	uint qualityMode,
	uint numBlockerSamples,
	float2 uv,
	float nonLinearDepth,
	float2 searchRegionRadiusUV,
	Texture2DArray<float> shadowArray,
	const uint shadowIdx)
{
	float blockerSum = 0;
	numBlockers = 0;

	for (uint i = 0; i < numBlockerSamples; ++i)
	{
		const float2 offset = GetPoissonBlockerSample(i, qualityMode) * searchRegionRadiusUV;
		const float depthSample = shadowArray.SampleLevel(WhiteBorderMinMagMipPoint, float3(uv + offset, shadowIdx), 0);
		if (depthSample < nonLinearDepth)
		{
			blockerSum += depthSample;
			numBlockers++;
		}
	}

	avgBlockerDepth = blockerSum / numBlockers;
}


// Performs PCF filtering on the shadow map using multiple taps in the penumbra filter region
float PCFPenumbra(
	uint qualityMode,
	uint numPenumbraSamples, 
	float2 uv, 
	float nonLinearDepth, 
	float2 filterRadiusUV,
	Texture2DArray<float> shadowArray,
	const uint shadowIdx)
{
	float sum = 0;
	
	for (uint i = 0; i < numPenumbraSamples; ++i)
	{
		const float2 offset = GetPoissonPenumbraSample(i, qualityMode) * filterRadiusUV;
		sum += shadowArray.SampleCmpLevelZero(BorderCmpMinMagLinearMipPoint, float3(uv + offset, shadowIdx), nonLinearDepth);
	}
	return sum / numPenumbraSamples;
}


float GetPCSSShadowFactor(
	uint qualityMode,
	uint numBlockerSamples,
	uint numPenumbraSamples,
	float2 shadowmapUVs,
	float nonLinearDepth, 
	float eyeDepth, 
	float2 shadowCamNearFar, 
	float2 lightUVRadiusSize,
	Texture2DArray<float> shadowArray,
	const uint shadowIdx)
{
	// Blocker search:
	float numBlockers = 0.f;
	float avgBlockerDepth = 0.f;
	const float2 searchRegionRadiusUV = SearchRegionRadiusUV(lightUVRadiusSize, shadowCamNearFar.x, eyeDepth);
	FindBlocker(
		avgBlockerDepth, // out
		numBlockers, // out
		qualityMode, 
		numBlockerSamples, 
		shadowmapUVs, 
		nonLinearDepth, 
		searchRegionRadiusUV,
		shadowArray,
		shadowIdx);

	if (numBlockers == 0.f)
	{
		return 1.f; // Early out if there are no blockers
	}

	// Compute the penumbra size:
	const float avgBlockerEyeDepth = 
		ConvertNonLinearDepthToLinear(shadowCamNearFar.x, shadowCamNearFar.y, avgBlockerDepth);
	const float2 penumbraRadiusUV = PenumbraRadiusUV(lightUVRadiusSize, eyeDepth, avgBlockerEyeDepth);
	const float2 filterRadiusUV = ProjectToLightUV(shadowCamNearFar.x, penumbraRadiusUV, eyeDepth);
	
	// Filter the result:
	return PCFPenumbra(
		qualityMode, numPenumbraSamples, shadowmapUVs, nonLinearDepth, filterRadiusUV, shadowArray, shadowIdx);
}


float GetPCFShadowFactor(
	float2 shadowmapUVs,
	float nonLinearDepth, 
	float4 shadowMapTexelSize, 
	Texture2DArray<float> shadowArray,
	const uint shadowIdx)
{
	// Compute a block of samples around our fragment, starting at the top-left. Note: MUST be a power of two.
	// TODO: Compute this on C++ side and allow for uploading of arbitrary samples (eg. odd, even)
	static const int gridSize = 4;

	const float offsetMultiplier = (float(gridSize) / 2.f) - 0.5f;

	shadowmapUVs.x -= offsetMultiplier * shadowMapTexelSize.z;
	shadowmapUVs.y += offsetMultiplier * shadowMapTexelSize.w;

	float depthSum = 0;
	for (uint row = 0; row < gridSize; row++)
	{
		for (uint col = 0; col < gridSize; col++)
		{
			depthSum += shadowArray.SampleCmpLevelZero(
				BorderCmpMinMagLinearMipPoint, float3(shadowmapUVs, shadowIdx), nonLinearDepth);
			
			shadowmapUVs.x += shadowMapTexelSize.z;
		}

		shadowmapUVs.x -= gridSize * shadowMapTexelSize.z;
		shadowmapUVs.y -= shadowMapTexelSize.w;
	}

	depthSum /= (gridSize * gridSize);
	
	return depthSum;
}


void GetBiasedShadowWorldPos(
	float3 worldPos, float3 worldNormal, float3 lightWDir, float2 minMaxShadowBias, out float3 shadowWorldPosOut)
{
	const float NoL = saturate(dot(worldNormal, lightWDir));
	
	shadowWorldPosOut = worldPos + (worldNormal * max(minMaxShadowBias.x, minMaxShadowBias.y * (1.f - NoL)));
}


float Get2DShadowFactor(
	float3 worldPos,
	float3 worldNormal,
	float3 lightWorldDir,
	float4x4 shadowCamVP,
	float2 shadowCamNearFar,
	float2 minMaxShadowBias,
	float shadowQualityMode,
	float2 lightUVRadiusSize,
	float4 shadowMapTexelSize,
	Texture2DArray<float> shadowArray,
	const uint shadowIdx)
{
	if (shadowIdx == INVALID_SHADOW_IDX)
	{
		return 1.f;
	}
	
	float3 biasedShadowWPos;
	GetBiasedShadowWorldPos(worldPos, worldNormal, lightWorldDir, minMaxShadowBias, biasedShadowWPos);
	
	// Transform our worldPos to shadow map projection space:
	float4 shadowProjPos = mul(shadowCamVP, float4(biasedShadowWPos, 1.f));
	shadowProjPos /= shadowProjPos.w; // Not necessary for orthogonal matrices
	
	// Shadow map UVs:
	float2 shadowmapUVs = (shadowProjPos.xy + 1.f) * 0.5f;
	shadowmapUVs.y = 1.f - shadowmapUVs.y; // Clip space Y+ is up, UV Y+ is down, so we flip Y here
	
	const float nonLinearDepth = shadowProjPos.z;
	const float eyeDepth = ConvertNonLinearDepthToLinear(shadowCamNearFar.x, shadowCamNearFar.y, nonLinearDepth);
	
	const uint shadowQualityModeUInt = (uint)shadowQualityMode;
	switch (shadowQualityModeUInt)
	{
		case 1: // PCSS low
		{
			// Note: Number of samples MUST match the poisson sample array size
			static const uint numBlockerSamples = 25;
			static const uint numPenumbraSamples = 25;
			
			return GetPCSSShadowFactor(
				shadowQualityModeUInt,
				numBlockerSamples, 
				numPenumbraSamples, 
				shadowmapUVs, 
				nonLinearDepth, 
				eyeDepth, 
				shadowCamNearFar, 
				lightUVRadiusSize,
				shadowArray,
				shadowIdx);
			}
		break;
		case 2: // PCSS high
		{
			static const uint numBlockerSamples = 32;
			static const uint numPenumbraSamples = 64;
			
			return GetPCSSShadowFactor(
				shadowQualityModeUInt,
				numBlockerSamples, 
				numPenumbraSamples,
				shadowmapUVs, 
				nonLinearDepth, 
				eyeDepth, 
				shadowCamNearFar, 
				lightUVRadiusSize,
				shadowArray,
				shadowIdx);
			}
		break;
		case 0: // PCF (lowest quality)
		default:
		{
				return GetPCFShadowFactor(shadowmapUVs, nonLinearDepth, shadowMapTexelSize, shadowArray, shadowIdx);
			}
	}
}


/**********************************************************************************************************************/


float2 ConvertUVRegionToRadians(float2 uv)
{
	return uv * M_PI_2; // The [0,1] cubemap face UVs span pi/2 radians
}


float3 GetOffsetCubeSampleDir(float3 cubeSampleDir, float cubeSampleDirLength, float2 uvOffset, float3x3 lookAtMatrix)
{	
	const float2 offsetRadians = ConvertUVRegionToRadians(uvOffset); // theta
	
	// Compute the length of the offsets along a plane perpendicular with our cubeSampleDir:
	const float2 offsetDistance = cubeSampleDirLength * tan(offsetRadians);
		
	// Rotate our offsets so they're on the plane perpendicular with our cubeSampleDir:
	const float3 rotatedOffsetDistance = mul(lookAtMatrix, float3(offsetDistance, 0.f));
	
	return cubeSampleDir + rotatedOffsetDistance;
}


// Find the number of samples between the shading point & light (blockers) & their average depth within the UV region
void FindCubeBlocker(
	out float avgBlockerDepth,
	out float numBlockers,
	uint qualityMode,
	uint numBlockerSamples,
	float3 cubeSampleDir,
	float cubeSampleDirLength,
	float3x3 lookAtMatrix,
	float nonLinearDepth,
	float2 searchRegionRadiusUV,
	TextureCubeArray<float> shadowArray,
	const uint shadowIdx)
{
	float blockerSum = 0;
	numBlockers = 0;
	
	for (uint i = 0; i < numBlockerSamples; ++i)
	{
		const float2 uvOffset = GetPoissonBlockerSample(i, qualityMode) * searchRegionRadiusUV;
		const float3 offsetDir = GetOffsetCubeSampleDir(cubeSampleDir, cubeSampleDirLength, uvOffset, lookAtMatrix);
		const float depthSample = shadowArray.SampleLevel(ClampMinMagMipPoint, float4(offsetDir, shadowIdx), 0);
		if (depthSample < nonLinearDepth)
		{
			blockerSum += depthSample;
			numBlockers++;
		}
	}

	avgBlockerDepth = blockerSum / numBlockers;
}


// Performs PCF filtering on the shadow map using multiple taps in the penumbra filter region
float PCFCubePenumbra(
	uint qualityMode,
	uint numPenumbraSamples,
	float3 cubeSampleDir,
	float cubeSampleDirLength,
	float3x3 lookAtMatrix,
	float nonLinearDepth,
	float2 filterRadiusUV,
	TextureCubeArray<float> shadowArray,
	const uint shadowIdx)
{
	float sum = 0;	
	
	for (uint i = 0; i < numPenumbraSamples; ++i)
	{
		const float2 uvOffset = GetPoissonPenumbraSample(i, qualityMode) * filterRadiusUV;
		const float3 offsetDir = GetOffsetCubeSampleDir(cubeSampleDir, cubeSampleDirLength, uvOffset, lookAtMatrix);
		
		sum += shadowArray.SampleCmpLevelZero(
			WrapCmpMinMagLinearMipPoint,
			float4(offsetDir, shadowIdx),
			nonLinearDepth);
	}
	return sum / numPenumbraSamples;
}


float GetCubePCSSShadowFactor(
	uint qualityMode,
	uint numBlockerSamples,
	uint numPenumbraSamples,
	float3 cubeSampleDir,
	float nonLinearDepth,
	float eyeDepth,
	float2 shadowCamNearFar,
	float2 lightUVRadiusSize,
	TextureCubeArray<float> shadowArray,
	const uint shadowIdx)
{	
	// Blocker search:
	float numBlockers = 0.f;
	float avgBlockerDepth = 0.f;
	const float2 searchRegionRadiusUV = SearchRegionRadiusUV(lightUVRadiusSize, shadowCamNearFar.x, eyeDepth);
	
	const float cubeSampleDirLength = length(cubeSampleDir);
	
	// Build a look-at matrix, to rotate samples to be oriented on a plane perpendicular with the sample direction:
	const float3x3 lookAtMatrix = BuildLookAtMatrix(cubeSampleDir);

	FindCubeBlocker(
		avgBlockerDepth, // out
		numBlockers, // out
		qualityMode,
		numBlockerSamples,
		cubeSampleDir,
		cubeSampleDirLength,
		lookAtMatrix,
		nonLinearDepth,
		searchRegionRadiusUV,
		shadowArray,
		shadowIdx);

	if (numBlockers == 0.f)
	{
		return 1.f; // Early out if there are no blockers
	}

	// Compute the penumbra size:
	const float avgBlockerEyeDepth =
		ConvertNonLinearDepthToLinear(shadowCamNearFar.x, shadowCamNearFar.y, avgBlockerDepth);
	
	const float2 penumbraRadiusUV = PenumbraRadiusUV(lightUVRadiusSize, eyeDepth, avgBlockerEyeDepth);
	const float2 filterRadiusUV = ProjectToLightUV(shadowCamNearFar.x, penumbraRadiusUV, eyeDepth);	
	
	// Filter the result:
	return PCFCubePenumbra(
		qualityMode,
		numPenumbraSamples, 
		cubeSampleDir, 
		cubeSampleDirLength, 
		lookAtMatrix, 
		nonLinearDepth, 
		filterRadiusUV,
		shadowArray,
		shadowIdx);
}


// Compute a PCF soft shadow factor from a cube map
// Based on Lengyel's Foundations of Game Engine Development Volume 2: Rendering, p164, listing 8.8
float GetCubePCFShadowFactor(
	float3 cubeSampleDir,
	float3 absSampleDir,
	float maxXY,
	float nonLinearDepth, 
	float eyeDepth, 
	float2 shadowCamNearFar, 
	float cubeFaceDim,
	TextureCubeArray<float> shadowArray,
	const uint shadowIdx)
{
	if (shadowIdx == INVALID_SHADOW_IDX)
	{
		return 1.f;
	}
	
	// Compute a sample offset for PCF shadow samples:
	const float sampleOffset = 2.f / cubeFaceDim;
	
	// Calculate offset vectors:
	const float offset = sampleOffset * eyeDepth;
	const float dxy = (maxXY > absSampleDir.z) ? offset : 0.f;
	const float dx = (absSampleDir.x > absSampleDir.y) ? dxy : 0.f;
	const float2 oxy = float2(offset - dx, dx);
	const float2 oyz = float2(offset - dxy, dxy);

	float3 limit = float3(eyeDepth, eyeDepth, eyeDepth);
	const float bias = 1.f / 1024.f; // Epsilon = 1/1024

	limit.xy -= oxy * bias;
	limit.yz -= oyz * bias;
	
	// Get the center sample:
	float light = shadowArray.SampleCmpLevelZero(
		WrapCmpMinMagLinearMipPoint,
		float4(cubeSampleDir, shadowIdx),
		nonLinearDepth);
	
	// Get 4 extra samples at diagonal offsets:
	cubeSampleDir.xy -= oxy;
	cubeSampleDir.yz -= oyz;

	light += shadowArray.SampleCmpLevelZero(
		WrapCmpMinMagLinearMipPoint,
		float4(clamp(cubeSampleDir, -limit, limit), shadowIdx),
		nonLinearDepth);
	cubeSampleDir.xy += oxy * 2.f;

	light += shadowArray.SampleCmpLevelZero(
		WrapCmpMinMagLinearMipPoint,
		float4(clamp(cubeSampleDir, -limit, limit), shadowIdx),
		nonLinearDepth);
	cubeSampleDir.yz += oyz * 2.f;

	light += shadowArray.SampleCmpLevelZero(
		WrapCmpMinMagLinearMipPoint,
		float4(clamp(cubeSampleDir, -limit, limit), shadowIdx),
		nonLinearDepth);
	cubeSampleDir.xy -= oxy * 2.f;

	light += shadowArray.SampleCmpLevelZero(
		WrapCmpMinMagLinearMipPoint,
		float4(clamp(cubeSampleDir, -limit, limit), shadowIdx),
		nonLinearDepth);

	return (light * 0.2); // Return the average of our 5 samples
}


float GetCubeShadowFactor(
	float3 worldPos,
	float3 worldNormal,
	float3 lightWorldPos,
	float3 lightWorldDir,
	float2 shadowCamNearFar, 
	float2 minMaxShadowBias,
	float shadowQualityMode,
	float2 lightUVRadiusSize,
	float cubeFaceDim,
	TextureCubeArray<float> shadowArray,
	const uint shadowIdx)
{
	float3 biasedShadowWPos;
	GetBiasedShadowWorldPos(worldPos, worldNormal, lightWorldDir, minMaxShadowBias, biasedShadowWPos);
	
	const float3 lightToPoint = biasedShadowWPos - lightWorldPos;
	float3 cubeSampleDir = WorldToCubeSampleDir(lightToPoint);
	
	// The eye depth w.r.t our cubemap view is the value of the largest component of the cubeSampleDir
	const float3 absSampleDir = abs(cubeSampleDir);
	const float maxXY = max(absSampleDir.x, absSampleDir.y);
	const float eyeDepth = max(maxXY, absSampleDir.z);
	
	// Calculate non-linear, projected depth buffer depth from the light-to-fragment direction
	const float nonLinearDepth =
		ConvertLinearDepthToNonLinear(shadowCamNearFar.x, shadowCamNearFar.y, eyeDepth);
	
	const uint shadowQualityModeUInt = (uint)shadowQualityMode;
	switch (shadowQualityModeUInt)
	{
		case 1: // PCSS low
		{
			// Note: Number of samples MUST match the poisson sample array size
			static const uint numBlockerSamples = 25;
			static const uint numPenumbraSamples = 25;
			
			return GetCubePCSSShadowFactor(
				shadowQualityModeUInt,
				numBlockerSamples,
				numPenumbraSamples,
				cubeSampleDir,
				nonLinearDepth,
				eyeDepth,
				shadowCamNearFar,
				lightUVRadiusSize,
				shadowArray,
				shadowIdx);
			}
		break;
		case 2: // PCSS high
		{
			static const uint numBlockerSamples = 32;
			static const uint numPenumbraSamples = 64;
			
			return GetCubePCSSShadowFactor(
				shadowQualityModeUInt,
				numBlockerSamples,
				numPenumbraSamples,
				cubeSampleDir,
				nonLinearDepth,
				eyeDepth,
				shadowCamNearFar,
				lightUVRadiusSize,
				shadowArray,
				shadowIdx);
			}
		break;
		case 0: // PCF (lowest quality)
		default:
		{
			return GetCubePCFShadowFactor(
				cubeSampleDir,
				absSampleDir,
				maxXY,
				nonLinearDepth, 
				eyeDepth, 
				shadowCamNearFar, 
				cubeFaceDim,
				shadowArray,
				shadowIdx);
		}
	}
}


#endif