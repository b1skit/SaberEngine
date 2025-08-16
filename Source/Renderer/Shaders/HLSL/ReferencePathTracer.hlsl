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
	
	const TemporalAccumulationData temporalAccumulationData = TemporalAccumulationParams[temporalAccumulationIdx];
	const uint numAccumulatedFrames = temporalAccumulationData.g_frameStats.x;
	
	const uint cameraParamsIdx = descriptorIndexes.g_descriptorIndexes0.z;
	const CameraData cameraParams = CameraParams[cameraParamsIdx];
	
	const uint2 pixelCoords = DispatchRaysIndex().xy;
	const uint2 screenDims = DispatchRaysDimensions().xy;
	
	RNGState2D rngState = InitializeRNGState2D(pixelCoords.xy, numAccumulatedFrames);
	
	RayDesc ray; // https://learn.microsoft.com/en-us/windows/win32/direct3d12/raydesc
	
	// Compute the ray origin and direction in world space:
	ray.Origin = cameraParams.g_cameraWPos;
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
	payload.g_pathRadiance = float4(0, 0, 0, 0);
	payload.g_rayDiff = rayDiff;
	
	// Trace the ray:
	TraceRay(
		SceneBVH[sceneBVHDescriptorIdx],	// TLAS
		traceRayParams.g_rayFlags.x,		// RayFlags
		traceRayParams.g_traceRayParams.x,	// InstanceInclusionMask
		traceRayParams.g_traceRayParams.y,	// RayContributionToHitGroupIndex
		traceRayParams.g_traceRayParams.z,	// MultiplierForGeometryContributionToHitGroupIndex
		traceRayParams.g_traceRayParams.w,	// Miss shader index
		ray,
		payload);
	
	const uint gOutputDescriptorIdx = descriptorIndexes.g_descriptorIndexes0.w;
	RWTexture2D<float4> outputTex = Texture2DRWFloat4[gOutputDescriptorIdx];
		
	// Compute a temporal cumulative average:	
	const float3 prevAccumulation = numAccumulatedFrames * outputTex[pixelCoords].rgb;
	const float3 newContribution = payload.g_pathRadiance.rgb;
		
	float3 newAverage = (prevAccumulation + newContribution) / (numAccumulatedFrames + 1.f);
		
	outputTex[pixelCoords] = float4(newAverage, 1.f);
}


[shader("closesthit")]
void ClosestHit(inout PathPayload payload, BuiltInTriangleIntersectionAttributes attrib)
{
	const float3 barycentrics = GetBarycentricWeights(attrib.barycentrics);
	
	const uint descriptorIndexesIdx = RootConstants0.g_data.z;
	const DescriptorIndexData descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
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
		LoadTriangleData(geoIdx, vertexStreamsLUTIdx, transformResourceIdx, transformBufferIdx);
	
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
		materialResourceIdx, 
		materialBufferIdx, 
		materialType);
	
	float3 colorOut = materialData.LinearAlbedo.rgb;
	
	payload.g_pathRadiance = float4(colorOut, RayTCurrent());
	payload.g_rayDiff = transferredRayDiff;
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
		LoadTriangleData(geoIdx, vertexStreamsLUTIdx, transformResourceIdx, transformBufferIdx);
	
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
		materialResourceIdx,
		materialBufferIdx,
		materialType);
	
	if (materialData.LinearAlbedo.a < materialData.AlphaCutoff)
	{
		IgnoreHit();
	}
}


[shader("miss")]
void Miss(inout PathPayload payload : SV_RayPayload)
{
	const uint descriptorIndexesIdx = RootConstants0.g_data.z;
	const DescriptorIndexData descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	const uint environmentMapIdx = descriptorIndexes.g_descriptorIndexes1.x;
	if (environmentMapIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> envMap = Texture2DFloat4[environmentMapIdx];

		uint3 texDims = uint3(0, 0, 0);
		envMap.GetDimensions(0.f, texDims.x, texDims.y, texDims.z);
		
		const float mipLevel = ComputeIBLTextureLOD(payload.g_rayDiff, texDims);
		
		const float2 uv = WorldDirToSphericalUV(WorldRayDirection());
		
		payload.g_pathRadiance = envMap.SampleLevel(WrapMinMagMipLinear, uv, mipLevel);
	}
	else
	{
		payload.g_pathRadiance = float4(135.f / 255.f, 206.f / 255.f, 235.f / 255.f, 1.f); // As per Skybox shader
	}
}