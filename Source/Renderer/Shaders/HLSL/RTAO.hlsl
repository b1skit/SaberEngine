// © 2025 Adam Badke. All rights reserved.
#include "BindlessResources.hlsli"
#include "RayTracingCommon.hlsli"
#include "UVUtils.hlsli"

#include "../Common/MaterialParams.h"
#include "../Common/RayTracingParams.h"
#include "../Common/RTAOParams.h"
#include "../Common/ResourceCommon.h"


// .x = SceneBVH idx, .y = TraceRayParams idx, .z = DescriptorIndexes, .w = unused
ConstantBuffer<RootConstantData> RootConstants0 : register(b0, space0);


[shader("closesthit")]
void RTAO_ClosestHit(inout RTAO_HitInfo payload, BuiltInTriangleIntersectionAttributes attrib)
{	
	// TODO: Handle transparent geo: Add 1 when transparent geometry is missed
}


[shader("anyhit")]
void RTAO_AnyHit(inout RTAO_HitInfo payload, BuiltInTriangleIntersectionAttributes attrib)
{
	// TODO: Handle transparent geo: Add 1 when transparent geometry is missed
}


[shader("miss")]
void RTAO_Miss(inout RTAO_HitInfo payload : SV_RayPayload)
{
	payload.g_visibility += 1.f; // Increment visibility when no geometry is hit
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
    const ConstantBuffer<DescriptorIndexData> descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
    const RTAOParamsData rtaoParams = RTAOParams[rtaoParamsIdx];
	
    const uint gbufferDepthIdx = rtaoParams.g_indexes.x;
    const uint gbufferNormalIdx = rtaoParams.g_indexes.y;
	
    const uint cameraParamsIdx = descriptorIndexes.g_descriptorIndexes.z;
    const CameraData cameraParams = CameraParams[cameraParamsIdx];
	
	// Convert the launch pixel coords to UV coordinates:
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const float2 dims = DispatchRaysDimensions().xy;
    const float2 uvs = PixelCoordsToScreenUV(launchIndex, dims, float2(0.5f, 0.5f));
	
	// Get the GBuffer depth:    
    Texture2D<float> gbufferDepthTex = Texture2DFloat[gbufferDepthIdx];
    const float nonLinearDepth = gbufferDepthTex.Load(uint3(launchIndex.xy, 0));
	
	// Get the world position:
    const float3 worldPos = ScreenUVToWorldPos(uvs, nonLinearDepth, cameraParams.g_invViewProjection);
	
	// Get the GBuffer normal:
    Texture2D<float4> gbufferNormalTex = Texture2DFloat4[gbufferNormalIdx];
    const float3 worldNormal = gbufferNormalTex.Load(uint3(launchIndex.xy, 0)).xyz;
	
	// Build our AO rays:
	RayDesc ray; // https://learn.microsoft.com/en-us/windows/win32/direct3d12/raydesc
	
    ray.Origin = worldPos;
    ray.Direction = worldNormal; // TODO: Use a random direction in a hemisphere around the normal
	
    ray.TMin = rtaoParams.g_params.x;
    ray.TMax = rtaoParams.g_params.y;
	
	// Initialize the ray payload
    RTAO_HitInfo payload;
    payload.g_visibility = 0.f;
	
	// Trace the ray
	TraceRay(
		sceneBVH,							// Acceleration structure
		traceRayParams.g_rayFlags.x,		// RayFlags
		traceRayParams.g_traceRayParams.x,	// InstanceInclusionMask
		traceRayParams.g_traceRayParams.y,	// RayContributionToHitGroupIndex
		traceRayParams.g_traceRayParams.z,	// MultiplierForGeometryContributionToHitGroupIndex
		traceRayParams.g_traceRayParams.w,	// MissShaderIndex
		ray,								// Ray to trace
		payload);							// Payload


	const uint outputDescriptorIdx = descriptorIndexes.g_descriptorIndexes.w;
	RWTexture2D<float> outputTex = Texture2DRWFloat[outputDescriptorIdx];
	
	outputTex[launchIndex].r = payload.g_visibility;
}