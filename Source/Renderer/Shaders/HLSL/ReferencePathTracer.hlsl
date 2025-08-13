// © 2025 Adam Badke. All rights reserved.
#include "BindlessResources.hlsli"
#include "RayTracingCommon.hlsli"
#include "GBufferBindless.hlsli"
#include "Random.hlsli"
#include "TextureLODHelpers.hlsli"

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
	
	const uint cameraParamsIdx = descriptorIndexes.g_descriptorIndexes.z;
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
	PathTracer_HitInfo payload;
	payload.g_colorAndDistance = float4(0, 0, 0, 0);
	payload.g_rayDiff = rayDiff;
	
	// Trace the ray:
	TraceRay(
		SceneBVH[sceneBVHDescriptorIdx],

		// Parameter name: RayFlags
		// Flags can be used to specify the behavior upon hitting a surface
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/ray_flag
		traceRayParams.g_rayFlags.x,

		// Parameter name: InstanceInclusionMask
		// Instance inclusion mask, which can be used to mask out some geometry to
		// this ray by and-ing the mask with a geometry mask. The 0xFF flag then
		// indicates no geometry will be masked
		traceRayParams.g_traceRayParams.x,

		// Parameter name: RayContributionToHitGroupIndex
		// Depending on the type of ray, a given object can have several hit
		// groups attached (ie. what to do when hitting to compute regular
		// shading, and what to do when hitting to compute shadows). Those hit
		// groups are specified sequentially in the SBT, so the value below
		// indicates which offset (on 4 bits) to apply to the hit groups for this
		// ray. In this sample we only have one hit group per object, hence an
		// offset of 0.
		traceRayParams.g_traceRayParams.y,

		// Parameter name: MultiplierForGeometryContributionToHitGroupIndex
		// The offsets in the SBT can be computed from the object ID, its instance
		// ID, but also simply by the order the objects have been pushed in the
		// acceleration structure. This allows the application to group shaders in
		// the SBT in the same order as they are added in the AS, in which case
		// the value below represents the stride (4 bits representing the number
		// of hit groups) between two consecutive objects.
		traceRayParams.g_traceRayParams.z,

		// Parameter name: MissShaderIndex
		// Index of the miss shader to use in case several consecutive miss
		// shaders are present in the SBT. This allows to change the behavior of
		// the program when no geometry have been hit, for example one to return a
		// sky color for regular rendering, and another returning a full
		// visibility value for shadow rays. This sample has only one miss shader,
		// hence an index 0
		traceRayParams.g_traceRayParams.w,

		ray,
		payload);

	
	const uint gOutputDescriptorIdx = descriptorIndexes.g_descriptorIndexes.w;
	RWTexture2D<float4> outputTex = Texture2DRWFloat4[gOutputDescriptorIdx];
		
	// Compute a temporal cumulative average:	
	const float3 prevAccumulation = numAccumulatedFrames * outputTex[pixelCoords].rgb;
	const float3 newContribution = payload.g_colorAndDistance.rgb;
		
	float3 newAverage = (prevAccumulation + newContribution) / (numAccumulatedFrames + 1.f);
		
	outputTex[pixelCoords] = float4(newAverage, 1.f);
}


[shader("closesthit")]
void ClosestHit(inout PathTracer_HitInfo payload, BuiltInTriangleIntersectionAttributes attrib)
{
	const float3 barycentrics = GetBarycentricWeights(attrib.barycentrics);
	
	const uint descriptorIndexesIdx = RootConstants0.g_data.z;
	const DescriptorIndexData descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	// Camera:
	const uint cameraParamsIdx = descriptorIndexes.g_descriptorIndexes.z;
	const CameraData cameraParams = CameraParams[cameraParamsIdx];	
	
	// Get our Vertex stream LUTs buffer:
	const uint vertexStreamsLUTIdx = descriptorIndexes.g_descriptorIndexes.x;
	
	// Compute our geometry index for buffer arrays aligned with AS geometry:
	const uint geoIdx = InstanceID() + GeometryIndex();
	
	const uint instancedBufferLUTIdx = descriptorIndexes.g_descriptorIndexes.y;
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
	
	float3 colorOut =
		hitData.m_hitColor.rgb *
		materialData.m_linearAlbedo.rgb *
		materialData.m_baseColorFactor.rgb;
	
	payload.g_colorAndDistance = float4(colorOut, RayTCurrent());
	payload.g_rayDiff = transferredRayDiff;
}


[shader("anyhit")]
void AnyHit(inout PathTracer_HitInfo payload, BuiltInTriangleIntersectionAttributes attrib)
{
	payload.g_colorAndDistance = float4(float3(1, 1, 0), RayTCurrent());
}


[shader("miss")]
void Miss(inout PathTracer_HitInfo payload : SV_RayPayload)
{
	uint2 pixelCoords = DispatchRaysIndex().xy;
	float2 dims = float2(DispatchRaysDimensions().xy);

	float ramp = pixelCoords.y / dims.y;
	
	payload.g_colorAndDistance = float4(0.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
}