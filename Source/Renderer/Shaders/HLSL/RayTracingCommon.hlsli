// © 2025 Adam Badke. All rights reserved.
#ifndef RAY_TRACING_COMMON_HLSLI
#define RAY_TRACING_COMMON_HLSLI

#include "MathConstants.hlsli"
#include "Transformations.hlsli"

#include "../Common/RayTracingParams.h"


struct VertexData
{
	float3 m_worldVertexPosition;
	float4 m_worldVertexTangent;
	float3 m_worldVertexNormal;		// Unnormalized vertex/geometry normal
	float2 m_vertexUV0;
	float2 m_vertexUV1;
	float4 m_vertexColor;
};
struct TriangleData
{
	VertexData m_v0;
	VertexData m_v1;
	VertexData m_v2;
	float3 m_worldTriPlaneNormal; // Normalized triangle plane normal (i.e. not interpolated)
};
struct TriangleHitData
{
	float3 m_worldHitPosition;
	float4 m_worldHitTangent;
	float3 m_worldHitNormal;	// Normalized vertex normal (i.e. interpolated from v0/v1/v2 geometry normals)
	float2 m_hitUV0;
	float2 m_hitUV1;
	float4 m_hitColor;	
};
struct BarycentricDerivatives
{
	float2 dU;
	float2 dV;
	float2 dW;
};


// Returns interpolation weights for v0, v1, v2 of a triangle
float3 GetBarycentricWeights(float2 bary)
{
	return float3(1.f - bary.x - bary.y, bary.x, bary.y);
}


// Returns a *normalized* ray direction in world space, given pixel coordinates, screen dimensions, camera position,
// and the inverse view-projection matrix. By default, the ray direction is offset by 0.5 pixels to center it in the
// pixel, but this can be adjusted/jittered with the 'offset' parameter [0,1]
float3 CreateViewRay(
	uint2 pixelCoords, uint2 screenDims, float3 camWorldPos, float4x4 invViewProjection, float2 offset = float2(0.5f, 0.5f))
{
	const float2 screenUV = (pixelCoords + offset) / screenDims.xy; // Top-left origin
	const float2 ndc = screenUV * 2.f - 1.f; // [-1, 1]
	
	const float4 ndcPoint = float4(ndc.x, -ndc.y, 1.f, 1.f); // NDC ray: Flip Y to compensate for the top-left UV origin
	
	float4 worldPoint = mul(invViewProjection, ndcPoint);
	worldPoint /= worldPoint.w; // Perspective divide
	
	return normalize(worldPoint.xyz - camWorldPos); // Note: Normalize to ensure RayTCurrent() is in world-space units
}


// Returns a *normalized* ray direction in world space, given pixel coordinates, screen dimensions, aspect ratio,
// tan(fovY/2), and the inverse view matrix. By default, the ray direction is offset by 0.5 pixels to center it in the
// pixel, but this can be adjusted/jittered with the 'offset' parameter [0,1]
float3 CreateViewRay(
	uint2 pixelCoords,
	uint2 screenDims,
	float4x4 invView,
	float aspectRatio,
	float tanHalfFovY, // tan(fovY/2)
	float2 offset = float2(0.5f, 0.5f))
{
	// Get the camera basis vectors:
	float3 X, Y, Z;
	GetCameraBasisVectors(invView, X, Y, Z);
	
	const float3 r = aspectRatio * tanHalfFovY * X; // Right vector at the near plane
	const float3 u = -tanHalfFovY * Y; // Up vector at the near plane (negated to account for top-left UV origin)

	const float3 v = -Z; // Forward vector (negated to point forward, in the direction the camera is looking in)
	const float3 D =	(((2.f * (pixelCoords.x + offset.x)) / (float) screenDims.x) - 1.f) * r +
						(((2.f * (pixelCoords.y + offset.y)) / (float) screenDims.y) - 1.f) * u +
						v;
	return normalize(D); // Note: Normalize to ensure RayTCurrent() is in world-space units
}


// Offset a ray origin to prevent self-intersections, considering the floating-point error with respect to distance from
// the origin.
// Normal points outward for rays exiting the surface, else is flipped.
// As per Ray Tracing Gems 2, Ch.6 "A fast and robust method for avoiding self-intersection", listing 6-1
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


float TraceShadowRayInline(
	RaytracingAccelerationStructure bvh,
	float3 origin,
	float3 direction,
	float3 geometryNormal, // i.e. interpolated vertex normal
	float tMin,
	float tMax,
	uint rayFlags, // Ray flags: Interally OR'd with QUERY_RAY_FLAGS
	uint instanceMask)
{
	origin = ComputeOriginOffset(origin, geometryNormal);
	
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = direction;
	ray.TMin = tMin;
	ray.TMax = tMax;

	// These ray query flags are OR'd with the ones provided in traceRayInlineParams (specified once for all draws), so
	// we set the minimum here
#define QUERY_RAY_FLAGS \
	RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | \
	RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | \
	RAY_FLAG_CULL_BACK_FACING_TRIANGLES
			
	RayQuery<QUERY_RAY_FLAGS> rayQuery;
			
	// Configure the trace:
	rayQuery.TraceRayInline(bvh, rayFlags, instanceMask, ray);

	// Execute the traversal:
	while (rayQuery.Proceed())
	{
		switch (rayQuery.CandidateType())
		{
		case CANDIDATE_PROCEDURAL_PRIMITIVE:
		{
			rayQuery.Abort(); // Procedural primitives are not currently supported
		}
		break;
		case CANDIDATE_NON_OPAQUE_TRIANGLE:
		{
			// TODO: Handle non-opaque triangles (e.g. alpha-tested)
			rayQuery.CommitNonOpaqueTriangleHit();
		}
		break;
		}
	}
	
	switch (rayQuery.CommittedStatus())
	{
	case COMMITTED_TRIANGLE_HIT:
	case COMMITTED_PROCEDURAL_PRIMITIVE_HIT:
	{
		return 0.f; // Hit: We're occluded
	}
	break;
	default:
	case COMMITTED_NOTHING:
	{
		return 1.f; // Miss: We're not occluded
	}
	}
}


#endif // RAY_TRACING_COMMON_HLSLI