// © 2025 Adam Badke. All rights reserved.
#include "RayTracingCommon.hlsli"

#include "../Common/CameraParams.h"
#include "../Common/RayTracingParams.h"


// This shader is currently based on copy/pastes from the DXR raytracing tutorial, so we have something valid to compile
// https://developer.nvidia.com/rtx/raytracing/dxr/dx12-raytracing-tutorial-part-1


//struct STriVertex
//{
//	float3 vertex;
//	float4 color;
//};

//StructuredBuffer<STriVertex> BTriVertex : register(t0, space0);
//StructuredBuffer<int> indices : register(t1);

// Raytracing output texture, accessed as a UAV
RWTexture2D<float4> gOutput : register(u0);

// Raytracing acceleration structure, accessed as a SRV
RaytracingAccelerationStructure SceneBVH : register(t0, space1);

ConstantBuffer<CameraData> CameraParams;
ConstantBuffer<TraceRayData> TraceRayParams;



[shader("closesthit")]
void ClosestHit_Experimental(inout HitInfo_Experimental payload, BuiltInTriangleIntersectionAttributes attrib)
{
	////float3 barycentrics =
	////  float3(1.f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);
	//const float3 barycentrics = GetBarycentricWeights(attrib.barycentrics);

	//uint vertId = 3 * PrimitiveIndex();
	//// #DXR Extra: Per-Instance Data
	//float3 hitColor = float3(0.6, 0.7, 0.6);
	//// Shade only the first 3 instances (triangles)
	//if (InstanceID() < 3)
	//{
	//	// #DXR Extra: Per-Instance Data
	//	hitColor = BTriVertex[indices[vertId + 0]].color.rgb * barycentrics.x +
	//		   BTriVertex[indices[vertId + 1]].color.rgb * barycentrics.y +
	//		   BTriVertex[indices[vertId + 2]].color.rgb * barycentrics.z;
	//}

	//payload.colorAndDistance = float4(hitColor, RayTCurrent());
	
	
	float3 colorOut = float3(0, 0, 0);
	
#if defined(OPAQUE_SINGLE_SIDED)
	colorOut = float3(0,1,0);
#elif defined(CLIP_SINGLE_SIDED)
	colorOut = float3(1,0,0);
#elif defined(OPAQUE_DOUBLE_SIDED)
	colorOut = float3(0,1,1);
#elif defined(CLIP_DOUBLE_SIDED)
	colorOut = float3(1,0,1);
#elif defined(BLEND_SINGLE_SIDED)
	colorOut = float3(1,1,0);
#elif defined(BLEND_DOUBLE_SIDED)
	colorOut = float3(1,1,1);
#endif
	
	payload.colorAndDistance = float4(colorOut, RayTCurrent());
}


[shader("anyhit")]
void AnyHit_Experimental(inout HitInfo_Experimental payload, BuiltInTriangleIntersectionAttributes attrib)
{
	////float3 barycentrics =
	////  float3(1.f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);
	//const float3 barycentrics = GetBarycentricWeights(attrib.barycentrics);

	//uint vertId = 3 * PrimitiveIndex();
	//// #DXR Extra: Per-Instance Data
	//float3 hitColor = float3(0.6, 0.7, 0.6);
	//// Shade only the first 3 instances (triangles)
	//if (InstanceID() < 3)
	//{
	//	// #DXR Extra: Per-Instance Data
	//	hitColor = BTriVertex[indices[vertId + 0]].color.rgb * barycentrics.x +
	//		   BTriVertex[indices[vertId + 1]].color.rgb * barycentrics.y +
	//		   BTriVertex[indices[vertId + 2]].color.rgb * barycentrics.z;
	//}

	//payload.colorAndDistance = float4(hitColor, RayTCurrent());
	
	payload.colorAndDistance = float4(float3(1, 1, 0), RayTCurrent());
}


[shader("raygeneration")]
void RayGeneration_Experimental()
{
	// Initialize the ray payload
	HitInfo_Experimental payload;
	payload.colorAndDistance = float4(0, 0, 0, 0);

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
	ray.Origin = mul(CameraParams.g_invView, float4(0, 0, 0, 1)).xyz;
	float4 target = mul(CameraParams.g_invProjection, float4(d.x, -d.y, 1, 1));
	ray.Direction = mul(CameraParams.g_invView, float4(target.xyz, 0)).xyz;
	ray.TMin = 0;
	ray.TMax = 100000;

	// Trace the ray
	TraceRay(
		// Parameter name: AccelerationStructure
		// Acceleration structure
		SceneBVH,

		// Parameter name: RayFlags
		// Flags can be used to specify the behavior upon hitting a surface
		// https://learn.microsoft.com/en-us/windows/win32/direct3d12/ray_flag
		TraceRayParams.g_rayFlags.x,

		// Parameter name: InstanceInclusionMask
		// Instance inclusion mask, which can be used to mask out some geometry to
		// this ray by and-ing the mask with a geometry mask. The 0xFF flag then
		// indicates no geometry will be masked
		TraceRayParams.g_traceRayParams.x,

		// Parameter name: RayContributionToHitGroupIndex
		// Depending on the type of ray, a given object can have several hit
		// groups attached (ie. what to do when hitting to compute regular
		// shading, and what to do when hitting to compute shadows). Those hit
		// groups are specified sequentially in the SBT, so the value below
		// indicates which offset (on 4 bits) to apply to the hit groups for this
		// ray. In this sample we only have one hit group per object, hence an
		// offset of 0.
		TraceRayParams.g_traceRayParams.y,

		// Parameter name: MultiplierForGeometryContributionToHitGroupIndex
		// The offsets in the SBT can be computed from the object ID, its instance
		// ID, but also simply by the order the objects have been pushed in the
		// acceleration structure. This allows the application to group shaders in
		// the SBT in the same order as they are added in the AS, in which case
		// the value below represents the stride (4 bits representing the number
		// of hit groups) between two consecutive objects.
		TraceRayParams.g_traceRayParams.z,

		// Parameter name: MissShaderIndex
		// Index of the miss shader to use in case several consecutive miss
		// shaders are present in the SBT. This allows to change the behavior of
		// the program when no geometry have been hit, for example one to return a
		// sky color for regular rendering, and another returning a full
		// visibility value for shadow rays. This sample has only one miss shader,
		// hence an index 0
		TraceRayParams.g_traceRayParams.w,

		// Parameter name: Ray
		// Ray information to trace
		ray,

		// Parameter name: Payload
		// Payload associated to the ray, which will be used to communicate
		// between the hit/miss shaders and the raygen
		payload);
	
#if defined(RAY_GEN_A)
	gOutput[launchIndex] = float4(payload.colorAndDistance.rgb, 1.f);
#endif
	
#if defined(RAY_GEN_B)
	const float scaleFactor = 0.5f;
	gOutput[launchIndex] = float4(payload.colorAndDistance.rgb * scaleFactor, 1.f);
#endif
}


[shader("miss")]
void Miss_Experimental(inout HitInfo_Experimental payload : SV_RayPayload)
{
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 dims = float2(DispatchRaysDimensions().xy);

	float ramp = launchIndex.y / dims.y;
	
#if defined(MISS_BLUE)
	payload.colorAndDistance = float4(0.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
#endif
	
#if defined(MISS_RED)
	payload.colorAndDistance = float4(1.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
#endif
}


[shader("callable")]
void Callable_Experimental(inout HitInfo_Experimental payload : SV_RayPayload)
{
	//
}