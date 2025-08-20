// © 2025 Adam Badke. All rights reserved.
#include "BindlessResources.hlsli"
#include "RayTracingCommon.hlsli"
#include "GBufferBindless.hlsli"
#include "Random.hlsli"
#include "TextureLODHelpers.hlsli"
#include "UVUtils.hlsli"

#include "../Common/MaterialParams.h"
#include "../Common/RayTracingParams.h"
#include "../Common/ResourceCommon.h"


// .x = SceneBVH idx, .y = TraceRayParams idx, .z = DescriptorIndexes, .w = unused
ConstantBuffer<RootConstantData> RootConstants0 : register(b0, space0);


[shader("raygeneration")]
void RayGeneration()
{
	const uint sceneBVHDescriptorIdx = RootConstants0.g_data.x;
	const uint traceRayParamsIdx = RootConstants0.g_data.y;
	const uint descriptorIndexesIdx = RootConstants0.g_data.z;
	const uint temporalAccumulationIdx = RootConstants0.g_data.w;
	
	const TraceRayData traceRayParams = TraceRayParams[traceRayParamsIdx];
	const DescriptorIndexData descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	const PathTracerData pathTracerData = PathTracerParams[temporalAccumulationIdx];
	const uint numAccumulatedFrames = pathTracerData.g_frameStats.x;
	const uint maxRays = pathTracerData.g_frameStats.y;
	
	const uint cameraParamsIdx = descriptorIndexes.g_descriptorIndexes0.z;
	const CameraData cameraParams = CameraParams[cameraParamsIdx];
	
	const uint2 pixelCoords = DispatchRaysIndex().xy;
	const uint2 screenDims = DispatchRaysDimensions().xy;
	
	RNGState2D rngState = InitializeRNGState2D(pixelCoords.xy, numAccumulatedFrames);
	
	RayDesc ray; // https://learn.microsoft.com/en-us/windows/win32/direct3d12/raydesc
	
	// Compute the initial ray origin and direction in world space:
	ray.Origin = cameraParams.g_cameraWPos.xyz;
	ray.Direction = CreateViewRay(
		pixelCoords, 
		screenDims, 
		cameraParams.g_cameraWPos.xyz, 
		cameraParams.g_invViewProjection,
		GetNextFloat2(rngState)); // Jitter the ray origin in the pixel
		
	ray.TMin = 0.f;
	ray.TMax = FLT_MAX;
	
	const RayDifferential rayDiff = CreateEyeRayDifferential(
		ray.Direction,
		cameraParams.g_invView,
		cameraParams.g_exposureProperties.w, // Aspect ratio
		cameraParams.g_exposureProperties.z, // tan(fovY/2)
		screenDims);
	
	// Initialize the ray payload
	PathPayload payload;
	payload.g_rayDiff = rayDiff;
	payload.g_worldHitPositionAndDistance = float4(cameraParams.g_cameraWPos.xyz, 0.f);
	payload.g_hitBarycentricsGeoPrimIdx = float4(0.f, 0.f, INVALID_RESOURCE_IDX, INVALID_RESOURCE_IDX);
	
	float4 pathRadiance = float4(0.f, 0.f, 0.f, 0.f);
	
	for (uint bounceIdx = 0; bounceIdx < maxRays; ++bounceIdx)
	{
		// Trace a ray to find the next point of intersection:
		if (payload.g_worldHitPositionAndDistance.w != FLT_MAX)
		{
			TraceRay(
				SceneBVH[sceneBVHDescriptorIdx],	// TLAS
				traceRayParams.g_rayFlags.x,		// RayFlags
				traceRayParams.g_traceRayParams.x,	// InstanceInclusionMask
				traceRayParams.g_traceRayParams.y,	// RayContributionToHitGroupIndex
				traceRayParams.g_traceRayParams.z,	// MultiplierForGeometryContributionToHitGroupIndex
				traceRayParams.g_traceRayParams.w,	// Miss shader index
				ray,
				payload);
		}
		
		// Hit:
		if (payload.g_worldHitPositionAndDistance.w != FLT_MAX)
		{
			const float3 barycentrics = GetBarycentricWeights(payload.g_hitBarycentricsGeoPrimIdx.xy);
		
			// Get our Vertex stream LUTs buffer:
			const uint vertexStreamsLUTIdx = descriptorIndexes.g_descriptorIndexes0.x;
	
			// Compute our geometry index for buffer arrays aligned with AS geometry:
			const uint geoIdx = payload.g_hitBarycentricsGeoPrimIdx.z;
			const uint primitiveIdx = payload.g_hitBarycentricsGeoPrimIdx.w;
	
			const uint instancedBufferLUTIdx = descriptorIndexes.g_descriptorIndexes0.y;
			const InstancedBufferLUTData instancedBuffersLUT = InstancedBufferLUTs[instancedBufferLUTIdx][geoIdx];
	
			const uint materialResourceIdx = instancedBuffersLUT.g_materialIndexes.x;
			const uint materialBufferIdx = instancedBuffersLUT.g_materialIndexes.y;
			const uint materialType = instancedBuffersLUT.g_materialIndexes.z;
	
			const uint transformResourceIdx = instancedBuffersLUT.g_transformIndexes.x;
			const uint transformBufferIdx = instancedBuffersLUT.g_transformIndexes.y;
	
			// Triangle data:
			const TriangleData triangleData =
				LoadTriangleData(geoIdx, primitiveIdx, vertexStreamsLUTIdx, transformResourceIdx, transformBufferIdx);
	
			// Interpolated triangle data at the hit point:
			const TriangleHitData hitData = GetTriangleHitData(triangleData, barycentrics);
			
			const RayDifferential transferredRayDiff = payload.g_rayDiff;
	
			// Material data:
			const MaterialData materialData = LoadMaterialData(
				triangleData,
				hitData,
				transferredRayDiff,
				ray.Direction,
				materialResourceIdx,
				materialBufferIdx,
				materialType);
	
			float3 colorOut = materialData.LinearAlbedo.rgb;
	
			pathRadiance += float4(colorOut, 1.f);
			
			// Update the ray for the next iteration:
			ray.Origin = ComputeOriginOffset(payload.g_worldHitPositionAndDistance.xyz, hitData.m_worldHitNormal);
			//ray.Direction = ; // TODO
		}
		else // Miss:
		{
			const uint environmentMapIdx = descriptorIndexes.g_descriptorIndexes1.x;
			pathRadiance = SampleEnvironmentMap(environmentMapIdx, payload.g_rayDiff, ray.Direction);
			break;
		}
	}
	
	const uint gOutputDescriptorIdx = descriptorIndexes.g_descriptorIndexes0.w;
	RWTexture2D<float4> outputTex = Texture2DRWFloat4[gOutputDescriptorIdx];
		
	// Compute a temporal cumulative average:	
	const float3 prevAccumulation = numAccumulatedFrames * outputTex[pixelCoords].rgb;
	const float3 newContribution = pathRadiance.rgb;
		
	float3 newAverage = (prevAccumulation + newContribution) / (numAccumulatedFrames + 1.f);
		
	outputTex[pixelCoords] = float4(newAverage, 1.f);
}


[shader("closesthit")]
void ClosestHit(inout PathPayload payload, BuiltInTriangleIntersectionAttributes attrib)
{	
	const uint descriptorIndexesIdx = RootConstants0.g_data.z;
	const DescriptorIndexData descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	// Get our Vertex stream LUTs buffer:
	const uint vertexStreamsLUTIdx = descriptorIndexes.g_descriptorIndexes0.x;
	
	// Compute our geometry index for buffer arrays aligned with AS geometry:
	const uint geoIdx = InstanceID() + GeometryIndex();
	
	const uint instancedBufferLUTIdx = descriptorIndexes.g_descriptorIndexes0.y;
	const InstancedBufferLUTData instancedBuffersLUT = InstancedBufferLUTs[instancedBufferLUTIdx][geoIdx];
	
	const uint transformResourceIdx = instancedBuffersLUT.g_transformIndexes.x;
	const uint transformBufferIdx = instancedBuffersLUT.g_transformIndexes.y;
	
	// Triangle data:
	const TriangleData triangleData =
		LoadTriangleData(geoIdx, PrimitiveIndex(), vertexStreamsLUTIdx, transformResourceIdx, transformBufferIdx);
	
	const RayDifferential transferredRayDiff = Transfer(
		triangleData,
		payload.g_rayDiff,
		WorldRayDirection(),
		RayTCurrent());
	
	// Update the payload:
	payload.g_rayDiff = transferredRayDiff;
	
	payload.g_worldHitPositionAndDistance =
		float4(WorldRayOrigin() + WorldRayDirection() * RayTCurrent(),
		RayTCurrent());
	
	payload.g_hitBarycentricsGeoPrimIdx = float4(attrib.barycentrics.xy, geoIdx, PrimitiveIndex());
}


[shader("anyhit")]
void AnyHit(inout PathPayload payload, BuiltInTriangleIntersectionAttributes attrib)
{
	const float3 barycentrics = GetBarycentricWeights(attrib.barycentrics);
	
	const uint descriptorIndexesIdx = RootConstants0.g_data.z;
	const DescriptorIndexData descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	// Camera:
	const uint cameraParamsIdx = descriptorIndexes.g_descriptorIndexes0.z;
	const CameraData cameraParams = CameraParams[cameraParamsIdx];
	
	// Get our Vertex stream LUTs buffer:
	const uint vertexStreamsLUTIdx = descriptorIndexes.g_descriptorIndexes0.x;
	
	// Compute our geometry index for buffer arrays aligned with AS geometry:
	const uint geoIdx = InstanceID() + GeometryIndex();
	
	const uint instancedBufferLUTIdx = descriptorIndexes.g_descriptorIndexes0.y;
	const InstancedBufferLUTData instancedBuffersLUT = InstancedBufferLUTs[instancedBufferLUTIdx][geoIdx];
	
	const uint materialResourceIdx = instancedBuffersLUT.g_materialIndexes.x;
	const uint materialBufferIdx = instancedBuffersLUT.g_materialIndexes.y;
	const uint materialType = instancedBuffersLUT.g_materialIndexes.z;
	
	const uint transformResourceIdx = instancedBuffersLUT.g_transformIndexes.x;
	const uint transformBufferIdx = instancedBuffersLUT.g_transformIndexes.y;
	
	// Triangle data:
	const TriangleData triangleData =
		LoadTriangleData(geoIdx, PrimitiveIndex(), vertexStreamsLUTIdx, transformResourceIdx, transformBufferIdx);
	
	// Interpolated triangle data at the hit point:
	const TriangleHitData hitData = GetTriangleHitData(triangleData, barycentrics);
	
	const RayDifferential transferredRayDiff = Transfer(
		triangleData,
		payload.g_rayDiff,
		WorldRayDirection(),
		RayTCurrent());
	
	// Material data:
	const MaterialData materialData = LoadMaterialData(
		triangleData,
		hitData,
		transferredRayDiff,
		WorldRayDirection(),
		materialResourceIdx,
		materialBufferIdx,
		materialType);
	
	// Alpha clipping:
	if (materialData.LinearAlbedo.a < materialData.AlphaCutoff)
	{
		IgnoreHit();
	}
}


[shader("miss")]
void Miss(inout PathPayload payload : SV_RayPayload)
{
	payload.g_worldHitPositionAndDistance.w = FLT_MAX; // Signal a miss
}