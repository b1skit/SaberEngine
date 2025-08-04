// © 2025 Adam Badke. All rights reserved.
#include "BindlessResources.hlsli"
#include "RayTracingCommon.hlsli"
#include "GBufferBindless.hlsli"

#include "../Common/MaterialParams.h"
#include "../Common/RayTracingParams.h"
#include "../Common/ResourceCommon.h"


// .x = SceneBVH idx, .y = TraceRayParams idx, .z = DescriptorIndexes, .w = unused
ConstantBuffer<RootConstantData> RootConstants0 : register(b0, space0);


[shader("closesthit")]
void ClosestHit(inout PathTracer_HitInfo payload, BuiltInTriangleIntersectionAttributes attrib)
{
	const float3 barycentrics = GetBarycentricWeights(attrib.barycentrics);
	
	const uint descriptorIndexesIdx = RootConstants0.g_data.z;
	const ConstantBuffer<DescriptorIndexData> descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	// Get our Vertex stream LUTs buffer:
	const uint vertexStreamsLUTIdx = descriptorIndexes.g_descriptorIndexes.x;
	const StructuredBuffer<VertexStreamLUTData> vertexStreamLUT = VertexStreamLUTs[vertexStreamsLUTIdx];	
	
	// Compute our geometry index for buffer arrays aligned with AS geometry:
	const uint geoIdx = InstanceID() + GeometryIndex();
	
	const uint instancedBufferLUTIdx = descriptorIndexes.g_descriptorIndexes.y;
	const StructuredBuffer<InstancedBufferLUTData> instancedBuffersLUT = InstancedBufferLUTs[instancedBufferLUTIdx];
	
	const uint materialResourceIdx = instancedBuffersLUT[geoIdx].g_materialIndexes.x;
	const uint materialBufferIdx = instancedBuffersLUT[geoIdx].g_materialIndexes.y;
	const uint materialType = instancedBuffersLUT[geoIdx].g_materialIndexes.z;
	
	const TriangleData triangleData = LoadTriangleData(geoIdx, vertexStreamsLUTIdx);
	const InterpolatedTriangleData interpolatedTriData = InterpolateTriangleData(triangleData, barycentrics);
	const MaterialData materialData = LoadMaterialData(
		interpolatedTriData, materialResourceIdx, materialBufferIdx, materialType);

	float3 colorOut = 
		interpolatedTriData.m_color.rgb *
		materialData.m_linearAlbedo.rgb * 
		materialData.m_baseColorFactor.rgb;
	
	payload.g_colorAndDistance = float4(colorOut, RayTCurrent());
}


[shader("anyhit")]
void AnyHit(inout PathTracer_HitInfo payload, BuiltInTriangleIntersectionAttributes attrib)
{
	payload.g_colorAndDistance = float4(float3(1, 1, 0), RayTCurrent());
}


[shader("raygeneration")]
void RayGeneration()
{
	// Initialize the ray payload
	PathTracer_HitInfo payload;
	payload.g_colorAndDistance = float4(0, 0, 0, 0);

	// Get the location within the dispatched 2D grid of work items
	// (often maps to pixels, so this could represent a pixel coordinate).
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 dims = float2(DispatchRaysDimensions().xy);
	float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f);
	
	// Define a ray, consisting of origin, direction, and the min-max distance
	// values
	// #DXR Extra: Perspective Camera
	float aspectRatio = dims.x / dims.y;
	
	// Perspective
	RayDesc ray; // https://learn.microsoft.com/en-us/windows/win32/direct3d12/raydesc
	
	const uint sceneBVHDescriptorIdx = RootConstants0.g_data.x;
	const uint traceRayParamsIdx = RootConstants0.g_data.y;
	const uint descriptorIndexesIdx = RootConstants0.g_data.z;
	
	const TraceRayData traceRayParams = TraceRayParams[traceRayParamsIdx];
	
	const ConstantBuffer<DescriptorIndexData> descriptorIndexes = DescriptorIndexes[descriptorIndexesIdx];
	
	const uint cameraParamsIdx = descriptorIndexes.g_descriptorIndexes.z;
	const CameraData cameraParams = CameraParams[cameraParamsIdx];
	
	ray.Origin = mul(cameraParams.g_invView, float4(0, 0, 0, 1)).xyz;
	float4 target = mul(cameraParams.g_invProjection, float4(d.x, -d.y, 1, 1));
	ray.Direction = mul(cameraParams.g_invView, float4(target.xyz, 0)).xyz;
	
	ray.TMin = 0;
	ray.TMax = 100000;
	
	// Trace the ray
	TraceRay(
		// Parameter name: AccelerationStructure
		// Acceleration structure
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

		// Parameter name: Ray
		// Ray information to trace
		ray,

		// Parameter name: Payload
		// Payload associated to the ray, which will be used to communicate
		// between the hit/miss shaders and the raygen
		payload);


	const uint gOutputDescriptorIdx = descriptorIndexes.g_descriptorIndexes.w;
	RWTexture2D<float4> outputTex = Texture2DRWFloat4[gOutputDescriptorIdx];
	
	outputTex[launchIndex] = payload.g_colorAndDistance;
}


[shader("miss")]
void Miss(inout PathTracer_HitInfo payload : SV_RayPayload)
{
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 dims = float2(DispatchRaysDimensions().xy);

	float ramp = launchIndex.y / dims.y;
	
	payload.g_colorAndDistance = float4(0.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
}