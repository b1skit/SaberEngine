// © 2025 Adam Badke. All rights reserved.
#include "../Common/RayTracingParams.h"


// Returns interpolation weights for v0, v1, v2 of a triangle
float3 GetBarycentricWeights(float2 bary)
{
	return float3(1.f - bary.x - bary.y, bary.x, bary.y);
}


float TraceShadowRay(
	RaytracingAccelerationStructure bvh, 
	ConstantBuffer<TraceRayInlineData> traceRayInlineParams,
	float3 origin,
	float3 direction,
	float tMin,
	float tMax)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = tMin;
	ray.TMax = tMax;

#define QUERY_RAY_FLAGS RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_CULL_BACK_FACING_TRIANGLES
			
	RayQuery<QUERY_RAY_FLAGS> rayQuery;
			
	// Configure the trace:
	rayQuery.TraceRayInline(
		bvh,
		QUERY_RAY_FLAGS | traceRayInlineParams.g_traceRayInlineParams.y, // Ray flags
		traceRayInlineParams.g_traceRayInlineParams.x, // Instance mask
		ray);
			
	// Execute the traversal:
	rayQuery.Proceed();
			
	if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
	{
		return 0.f; // Hit: We're occluded
	}
	return 1.f; // Miss: We're not occluded
}