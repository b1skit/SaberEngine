// © 2025 Adam Badke. All rights reserved.
#include "BindlessResources.hlsli"
#include "Random.hlsli"
#include "RayTracingCommon.hlsli"
#include "Sampling.hlsli"
#include "UVUtils.hlsli"

#include "../Common/MaterialParams.h"
#include "../Common/RayTracingParams.h"
#include "../Common/RTAOParams.h"
#include "../Common/ResourceCommon.h"


// .x = SceneBVH idx, .y = TraceRayParams idx, .z = DescriptorIndexes, .w = unused
ConstantBuffer<RootConstantData> RootConstants0 : register(b0, space0);


[shader("anyhit")]
void RTAO_AnyHit(inout RTAO_HitInfo hitInfo, BuiltInTriangleIntersectionAttributes attrib)
{
	// TODO: Handle transparent geo: Add 1 when transparent geometry is missed
	
	hitInfo.g_visibility = 0.f; // Increment visibility when no geometry is hit
}


[shader("miss")]
void RTAO_Miss(inout RTAO_HitInfo hitInfo : SV_RayPayload)
{
	hitInfo.g_visibility = 1.f; // Increment visibility when no geometry is hit
}


[shader("raygeneration")]
void RTAO_RayGeneration()
{	
	// Unpack root constant indices:
    const uint sceneBVHDescriptorIdx = RootConstants0.g_data.x;
    const uint traceRayParamsIdx = RootConstants0.g_data.y;
    const uint descriptorIndexesIdx = RootConstants0.g_data.z;
    const uint rtaoParamsIdx = RootConstants0.g_data.w;
	
	// Get resources from our bindless arrays:
    RaytracingAccelerationStructure sceneBVH = SceneBVH[sceneBVHDescriptorIdx];
    const TraceRayData traceRayParams = TraceRayParams[traceRayParamsIdx];
    const DescriptorIndexData descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
    const RTAOParamsData rtaoParams = RTAOParams[rtaoParamsIdx];
	
    const uint gbufferDepthIdx = rtaoParams.g_indexes.x;
    const uint gbufferNormalIdx = rtaoParams.g_indexes.y;	
	
    const uint cameraParamsIdx = descriptorIndexes.g_descriptorIndexes0.z;
    const CameraData cameraParams = CameraParams[cameraParamsIdx];
	
	// Convert the launch pixel coords to UV coordinates:
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 dims = DispatchRaysDimensions().xy;
    const float2 uvs = PixelCoordsToScreenUV(launchIndex, dims, float2(0.5f, 0.5f));
	
	const uint3 loadCoords = uint3(launchIndex.xy, 0);
	
	// Get the GBuffer depth:    
    Texture2D<float> gbufferDepthTex = Texture2DFloat[gbufferDepthIdx];
	const float nonLinearDepth = gbufferDepthTex.Load(loadCoords);
	
	// Get the world position:
    const float3 worldPos = ScreenUVToWorldPos(uvs, nonLinearDepth, cameraParams.g_invViewProjection);
	
	// Get the GBuffer normal:
    Texture2D<float4> gbufferNormalTex = Texture2DFloat4[gbufferNormalIdx];
	const float3 worldNormal = gbufferNormalTex.Load(loadCoords).xyz;
	
	// Get our output target:
	const uint outputDescriptorIdx = descriptorIndexes.g_descriptorIndexes0.w;
	RWTexture2D<float> outputTex = Texture2DRWFloat[outputDescriptorIdx];
	
	const uint numRays = rtaoParams.g_params.z;
	const bool isEnabled = rtaoParams.g_params.w > 0.5f;
	
	// If we have no rays, return early:	
	if (numRays == 0 || isEnabled == false)
	{
		outputTex[launchIndex].r = 1.f;
		return;
	}
	
	const Referential localReferential = BuildReferential(worldNormal, float3(0, 1, 0));
		
	// Note: We ignore the frame index as we don't temporally accumulate
	RNGState1D sampleGen = InitializeRNGState1D(launchIndex.xy, 0.f); 
	
	float visibility = 0.f;
	for (uint i = 0; i < numRays; i++)
	{
		const float angularOffset = GetNextFloat(sampleGen);
		
		float3 sampleDir;
		float NoL;
		float pdf;
		ImportanceSampleFibonacciSpiralDir(i, numRays, angularOffset, localReferential, sampleDir, NoL, pdf);
		
		// Build our AO rays:
		RayDesc ray; // https://learn.microsoft.com/en-us/windows/win32/direct3d12/raydesc
		ray.Origin = worldPos;
		ray.TMin = rtaoParams.g_params.x;
		ray.TMax = rtaoParams.g_params.y;
		ray.Direction = normalize(sampleDir);
	
		// Initialize the ray hitInfo:
		RTAO_HitInfo hitInfo;
		hitInfo.g_visibility = 0.f;
		
		// Trace the ray
		TraceRay(
			sceneBVH, // Acceleration structure
			traceRayParams.g_rayFlags.x, // RayFlags
			traceRayParams.g_traceRayParams.x, // InstanceInclusionMask
			traceRayParams.g_traceRayParams.y, // RayContributionToHitGroupIndex
			traceRayParams.g_traceRayParams.z, // MultiplierForGeometryContributionToHitGroupIndex
			traceRayParams.g_traceRayParams.w, // MissShaderIndex
			ray, // Ray to trace
			hitInfo); // Payload
		
		visibility += hitInfo.g_visibility;
	}
	
	const float visibilityFactor = visibility / numRays;

	outputTex[launchIndex].r = visibilityFactor;
}