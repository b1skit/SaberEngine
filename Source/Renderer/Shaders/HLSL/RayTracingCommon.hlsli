// © 2025 Adam Badke. All rights reserved.
#ifndef RAY_TRACING_COMMON_HLSLI
#define RAY_TRACING_COMMON_HLSLI

#include "MathConstants.hlsli"

#include "../Common/RayTracingParams.h"


// Returns interpolation weights for v0, v1, v2 of a triangle
float3 GetBarycentricWeights(float2 bary)
{
	return float3(1.f - bary.x - bary.y, bary.x, bary.y);
}


// Returns a ray direction in world space, given pixel coordinates, screen dimensions, camera position, and the inverse
// view-projection matrix. By default, the ray direction is offset by 0.5 pixels to center it in the pixel, but this can
// be adjusted/jittered with the 'offset' parameter.
float3 GetViewRay(uint2 pixelCoords, uint2 screenDims, float3 camWorldPos, float4x4 invViewProjection, float offset = 0.5f)
{
	const float2 screenUV = (pixelCoords + offset) / screenDims.xy; // Centered pixel UVs, top-left origin
	const float2 ndc = screenUV * 2.f - 1.f; // [-1, 1]
	
	const float4 ndcPoint = float4(ndc.x, -ndc.y, 1.f, 1.f); // NDC ray: Flip Y to compensate for the top-left UV origin
	
	float4 worldPoint = mul(invViewProjection, ndcPoint);
	worldPoint /= worldPoint.w; // Perspective divide
	
	return worldPoint.xyz - camWorldPos;
}


// Offset a ray origin to prevent self-intersections, considering the floating-point error with respect to distance from
// the origin.
// Normal points outward for rays exiting the surface, else is flipped.
// As per Ray Tracing Gems 2, Ch.6 "A fast and robust method for avoiding self-intersection", listing 6-1,
float3 ComputeOriginOffset(float3 p, float3 n)
{
#define ORIGIN 1.f / 32.f
#define FLOAT_SCALE 1.f / 65536.f
#define INT_SCALE 256.f
	
	const int3 of_i = int3(INT_SCALE * n.x, INT_SCALE * n.y, INT_SCALE * n.z);

	const float3 p_i = float3(
        asfloat(asint(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
        asfloat(asint(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
        asfloat(asint(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

	return float3(
        (abs(p.x) < ORIGIN) ? p.x + FLOAT_SCALE * n.x : p_i.x,
        (abs(p.y) < ORIGIN) ? p.y + FLOAT_SCALE * n.y : p_i.y,
        (abs(p.z) < ORIGIN) ? p.z + FLOAT_SCALE * n.z : p_i.z);
}


float TraceShadowRay(
	RaytracingAccelerationStructure bvh,
	ConstantBuffer<TraceRayInlineData> traceRayInlineParams,
	float3 origin,
	float3 direction,
	float3 geometryNormal, // i.e. interpolated vertex normal
	float tMin,
	float tMax)
{
	origin = ComputeOriginOffset(origin, geometryNormal);
	
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = tMin;
	ray.TMax = tMax;

#define QUERY_RAY_FLAGS \
	RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | \
	RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | \
	RAY_FLAG_CULL_BACK_FACING_TRIANGLES
			
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


#endif // RAY_TRACING_COMMON_HLSLI