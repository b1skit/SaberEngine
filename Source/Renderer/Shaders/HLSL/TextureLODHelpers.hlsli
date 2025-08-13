// © 2025 Adam Badke. All rights reserved.
#ifndef TEXTURE_LOD_HELPERS
#define TEXTURE_LOD_HELPERS

#include "MathConstants.hlsli"
#include "Transformations.hlsli"

#include "../Common/RayTracingParams.h"


// As per 3.2.1 Ray Tracing Gems, Ch 20. "Texture Level of Detail Strategies for Real-Time Ray Tracing"
RayDifferential CreateEyeRayDifferential(
	float3 D,			// Normalized ray direction in world space
	float4x4 invView,
	float aspectRatio,
	float tanHalfFovY,	// tan(fovY/2)
	uint2 screenDims, 
	float2 offset = float2(0.5f, 0.5f)) // [0,1], with 0.5 = pixel center
{
	// Get the camera basis vectors:
	float3 X, Y, Z;
	GetCameraBasisVectors(invView, X, Y, Z);
	
	const float3 r = aspectRatio * tanHalfFovY * X; // Right vector at the near plane
	const float3 u = -tanHalfFovY * Y; // Up vector at the near plane (negated to account for top-left UV origin)
	const float3 v = -Z; // Forward vector (negated to point forward, in the direction the camera is looking in)
	
	const float3 right = ((2.f * aspectRatio * tanHalfFovY) / screenDims.x) * X; // Right vector to next pixel
	const float3 up = (-2.f * tanHalfFovY / screenDims.y) * Y; // Up vector to next pixel
		
	// Our ray direction is normalized (to ensure RayTCurrent() is in world-space units), we must re-scale it here as
	// otherwise dot(D, D) always equals 1.
	D /= dot(D, v);
	
	const float DoD = dot(D, D);
	const float DoD_32 = DoD * sqrt(DoD); // dot(D, D)^(3/2)
	
	const float3 dDdx = (DoD * right - dot(D, right) * D) / DoD_32; // Derivative of D w.r.t. x
	const float3 dDdy = (DoD * up - dot(D, up) * D) / DoD_32; // Derivative of D w.r.t. y
	
	RayDifferential rayDiff;
	
	rayDiff.dOdx = float3(0.f, 0.f, 0.f);
	rayDiff.dOdy = float3(0.f, 0.f, 0.f);
	
	rayDiff.dDdx = dDdx;
	rayDiff.dDdy = dDdy;
	
	return rayDiff;
}


BarycentricDerivatives ComputeBarycentricDerivatives(
	RayDifferential transferredRayDiff, // Transferred ray differentials at the hit point
	float3 D,							// Ray direction (normalized/unnormalized - both are ok) arriving at the hit point
	TriangleData triangleData)
{
	// Triangle edge vectors:	
	const float3 e1 = triangleData.m_v1.m_worldVertexPosition - triangleData.m_v0.m_worldVertexPosition;
	const float3 e2 = triangleData.m_v2.m_worldVertexPosition - triangleData.m_v0.m_worldVertexPosition;
	
	// Vectors perpendicular to the ray direction D and one triangle edge:
	const float3 c_u = cross(e2, D);
	const float3 c_v = cross(D, e1);
	
	// Ray differentials at the hit point, (renamed to match the paper notation):
	const float3 q = transferredRayDiff.dOdx;
	const float3 r = transferredRayDiff.dOdy;
	
	// Cramer's rule: k is the determinant of the system
	const float k = dot(cross(e1, e2), D);
	const float rcpK = 1.f / (abs(k) < FLT_MIN ?
		(FLT_MIN * (k > 0.f ? 1.f : -1.f)) : // Guarantee non-zero denominator
		k);
	
	// Compute the barycentric derivatives:
	const float dudx = rcpK * dot(c_u, q);
	const float dudy = rcpK * dot(c_u, r);
	
	const float dvdx = rcpK * dot(c_v, q);
	const float dvdy = rcpK * dot(c_v, r);
	
	const float dwdx = -(dudx + dvdx);
	const float dwdy = -(dudy + dvdy);
	
	BarycentricDerivatives result;
	
	result.dU = float2(dudx, dudy);
	result.dV = float2(dvdx, dvdy);
	result.dW = float2(dwdx, dwdy);
	
	return result;
}


// Compute the derivatives of any float2 vertex data using barycentric derivatives
void ComputeVertexDataDifferentials(
	BarycentricDerivatives barycentricDerivatives,
	float2 data0, // Vertex 0 attribute to interpolate
	float2 data1,
	float2 data2,
	out float2 dxOut, // Out: Differential of the interpolated data w.r.t. x
	out float2 dyOut) // Out: Differential of the interpolated data w.r.t. y
{
	const float2 delta1 = data1 - data0; // Difference between v1 and v0
	const float2 delta2 = data2 - data0; // Difference between v2 and v0
	dxOut = barycentricDerivatives.dU.x * delta1 + barycentricDerivatives.dV.x * delta2; // Change w.r.t. x
	dyOut = barycentricDerivatives.dU.y * delta1 + barycentricDerivatives.dV.y * delta2; // Change w.r.t. y
}


// Compute the derivatives of any float3 vertex data using barycentric derivatives
void ComputeVertexDataDifferentials(
	BarycentricDerivatives barycentricDerivatives,
	float3 data0, // Vertex 0 attribute to interpolate
	float3 data1,
	float3 data2,
	out float3 dxOut, // Out: Differential of the interpolated data w.r.t. x
	out float3 dyOut) // Out: Differential of the interpolated data w.r.t. y
{
	const float3 delta1 = data1 - data0; // Difference between v1 and v0
	const float3 delta2 = data2 - data0; // Difference between v2 and v0
	dxOut = barycentricDerivatives.dU.x * delta1 + barycentricDerivatives.dV.x * delta2; // Change w.r.t. x
	dyOut = barycentricDerivatives.dU.y * delta1 + barycentricDerivatives.dV.y * delta2; // Change w.r.t. y
}


// As per section 3.2, "Tracing Ray Differentials" by Igehy et al. 1999
void ComputeVertexNormalDifferentials(
	float3 barycentrics,
	BarycentricDerivatives barycentricDerivatives,
	TriangleData triangleData,
	TriangleHitData hitData,
	out float3 dNdx,								// Out: Differential of the interpolated vertex normal w.r.t. x
	out float3 dNdy)								// Out: Differential of the interpolated vertex normal w.r.t. y
{
	// Get the derivatives of the *unnormalized* world vertex normals for the triangle:
	float3 dndx, dndy;
	ComputeVertexDataDifferentials(
		barycentricDerivatives,
		triangleData.m_v0.m_worldVertexNormal,
		triangleData.m_v1.m_worldVertexNormal,
		triangleData.m_v2.m_worldVertexNormal,
		dndx,
		dndy);
	
	// Compute Igehy et al.'s "n", the unnormalized vertex normal at the hit point.
	// Note: We specifically use the unnormalized vertex normals here, otherwise we always get dot(n,n) = 1 
	const float3 interpolatedVertexNormal =
		triangleData.m_v0.m_worldVertexNormal * barycentrics.x +
		triangleData.m_v1.m_worldVertexNormal * barycentrics.y +
		triangleData.m_v2.m_worldVertexNormal * barycentrics.z;
	
	const float non = dot(interpolatedVertexNormal, interpolatedVertexNormal);
	const float non_32 = non * sqrt(non); // dot(n, n)^(3/2)
	const float rcpNon_32 = 1.f / max(non_32, FLT_MIN);
	
	dNdx = (non * dndx - (dot(interpolatedVertexNormal, dndx) * interpolatedVertexNormal)) * rcpNon_32;
	dNdy = (non * dndy - (dot(interpolatedVertexNormal, dndy) * interpolatedVertexNormal)) * rcpNon_32;
}




// Propagate ray differentials through a homogeneous medium to the point of intersection with a surface.
// As per section 3.1.1, "Tracing Ray Differentials" by Igehy et al. 1999
RayDifferential Transfer(
	TriangleData triangleData,
	RayDifferential srcRayDiff, // Ray differential at the source of the ray
	float3 D,					// Normalized ray direction in world space
	float t)					// Distance from srcRayDiff.O to the point of intersection (world units)
{
	// Triangle plane normal:
	const float3 N = triangleData.m_worldTriPlaneNormal;
	
	// Avoid division by zero, but preserve the sign of DoN
	const float DoN = dot(D, N);
	const float rcpDoN = 1.f / (max(abs(DoN), FLT_MIN) * (DoN >= 0.f ? 1.f : -1.f)); // Ensure DoN is not zero
	
	const float3 t_dDdx = (t * srcRayDiff.dDdx);
	const float3 t_dDdy = (t * srcRayDiff.dDdy);
	
	const float3 dOdxPlusT_dDdx = srcRayDiff.dOdx + t_dDdx;
	const float3 dOdyPlusT_dDdy = srcRayDiff.dOdy + t_dDdy;
	
	const float dtdx = -dot(dOdxPlusT_dDdx, N) * rcpDoN; // Derivative of t w.r.t. x
	const float dtdy = -dot(dOdyPlusT_dDdy, N) * rcpDoN; // Derivative of t w.r.t. y
	
	RayDifferential result;
	
	result.dOdx = (dOdxPlusT_dDdx) + (dtdx * D); // q: Derivative of ray origin w.r.t x at intersection point
	result.dOdy = (dOdyPlusT_dDdy) + (dtdy * D); // r: Derivative of ray origin w.r.t y at intersection point
	
	result.dDdx = srcRayDiff.dDdx; // Ray direction does not change at the point of intersection for Transfer
	result.dDdy = srcRayDiff.dDdy;
	
	return result;
}


// Reflect ray differentials that have been transferred to at point of intersection with a surface
// As per section 3.1.2, "Tracing Ray Differentials" by Igehy et al. 1999
RayDifferential Reflect(
	float3 barycentrics,
	BarycentricDerivatives barycentricDerivatives,
	TriangleData triangleData,
	TriangleHitData hitData,
	RayDifferential transferredRayDiff,
	float3 D)										// Normalized ray direction in world space
{
	float3 dNdx, dNdy;
	ComputeVertexNormalDifferentials(barycentrics, barycentricDerivatives, triangleData, hitData, dNdx, dNdy);
	
	const float3 N = hitData.m_worldHitNormal; // == normalize(interpolatedVertexNormal)
	const float DoN = dot(D, N);
	
	const float dDoNdx = dot(transferredRayDiff.dDdx, N) + dot(D, dNdx); // Derivative of DoN w.r.t. x
	const float dDoNdy = dot(transferredRayDiff.dDdy, N) + dot(D, dNdy); // Derivative of DoN w.r.t. y
	
	RayDifferential result;
	
	result.dOdx = transferredRayDiff.dOdx;
	result.dOdy = transferredRayDiff.dOdy;
	
	result.dDdx = transferredRayDiff.dDdx - (2.f * (DoN * dNdx + dDoNdx * N));
	result.dDdy = transferredRayDiff.dDdy - (2.f * (DoN * dNdy + dDoNdy * N));
	
	return result;
}


// As per section 3.1.3, "Tracing Ray Differentials" by Igehy et al. 1999
RayDifferential Refract(
	float3 barycentrics,
	BarycentricDerivatives barycentricDerivatives,
	TriangleData triangleData,
	TriangleHitData hitData,
	float incidentIOR,					// Incident index of refraction
	float transmittedIOR,				// Transmitted index of refraction
	RayDifferential transferredRayDiff,
	float3 D)							// Normalized ray direction in world space
{
	const float nu = incidentIOR / transmittedIOR; // Index of refraction ratio
	
	float3 dNdx, dNdy;
	ComputeVertexNormalDifferentials(barycentrics, barycentricDerivatives, triangleData, hitData, dNdx, dNdy);
	
	const float3 N = hitData.m_worldHitNormal; // Normalized vertex normal at the hit point
	
	const float DoN = dot(D, N);
	
	const float DprimeoN = -sqrt(1.f - (nu * nu) * (1.f - (DoN * DoN))); // Igehy et al. (17)
	const float mu = nu * DoN - DprimeoN;
	
	const float dDoNdx = dot(transferredRayDiff.dDdx, N) + dot(D, dNdx); // Derivative of DoN w.r.t. x
	const float dDoNdy = dot(transferredRayDiff.dDdy, N) + dot(D, dNdy); // Derivative of DoN w.r.t. y
	
	const float dmudx = (nu - (nu * nu * DoN) / DprimeoN) * dDoNdx; // Igehy et al. (19)
	const float dmudy = (nu - (nu * nu * DoN) / DprimeoN) * dDoNdy;
	
	RayDifferential result;
	
	result.dOdx = transferredRayDiff.dOdx;
	result.dOdy = transferredRayDiff.dOdy;
	
	result.dDdx = nu * transferredRayDiff.dDdx - (mu * dNdx + dmudx * N); // Igehy et al. (18)
	result.dDdy = nu * transferredRayDiff.dDdy - (mu * dNdy + dmudy * N);
	
	return result;
}


// As per Ray Tracing Gems, Ch 20. "Texture Level of Detail Strategies for Real-Time Ray Tracing" (21)
float ComputeIsotropicTextureLOD(
	uint2 texDims,									// WxH texture dimensions (in pixels)
	BarycentricDerivatives barycentricDerivatives,
	TriangleData triangleData,						// Intersected triangle
	uint uvChannel)
{
	float2 g1, g2; // Differences in texture coordinates between the triangle vertices
	switch (uvChannel)
	{
	case 1:
	{
		g1 = triangleData.m_v1.m_vertexUV1 - triangleData.m_v0.m_vertexUV1;
		g2 = triangleData.m_v2.m_vertexUV1 - triangleData.m_v0.m_vertexUV1;
	}
	break;
	case 0:
	default:
	{
		g1 = triangleData.m_v1.m_vertexUV0 - triangleData.m_v0.m_vertexUV0;
		g2 = triangleData.m_v2.m_vertexUV0 - triangleData.m_v0.m_vertexUV0;
	}
	break;
	}
	
	// Compute the texture-space derivatives from the barycentric derivatives:
	const float dsdx = (float)texDims.x * ((barycentricDerivatives.dU.x * g1.x) + (barycentricDerivatives.dV.x * g2.x));
	const float dtdx = (float)texDims.y * ((barycentricDerivatives.dU.x * g1.y) + (barycentricDerivatives.dV.x * g2.y));
	
	const float dsdy = (float)texDims.x * ((barycentricDerivatives.dU.y * g1.x) + (barycentricDerivatives.dV.y * g2.x));
	const float dtdy = (float)texDims.y * ((barycentricDerivatives.dU.y * g1.y) + (barycentricDerivatives.dV.y * g2.y));
	
	const float dTxodTx = (dsdx * dsdx) + (dtdx * dtdx); // |dT/dx|^2 (i.e. length squared)
	const float dTyodTy = (dsdy * dsdy) + (dtdy * dtdy); // |dT/dy|^2
	
	// Note: The input to log2 will be very small here, so must use FLT_MIN
	return 0.5f * log2(max(max(dTxodTx, dTyodTy), FLT_MIN)); // LOD = log2(max(sqrt(dTx o dTx), sqrt(dTy o dTy)))
}


// As per Ray Tracing Gems, Ch 21. "Simple Environment Map Filtering Using Ray Cones and Ray Differentials"
float ComputeIBLTextureLOD(
	RayDifferential transferredRayDiff,
	uint2 texDims) // Spherical (latitude/longitude) IBL's WxH texture dimensions (in pixels)
{
	const float gamma = 2.f * atan(0.5f * length(transferredRayDiff.dDdx + transferredRayDiff.dDdy));
	
	const float radiansPerTexel = M_PI / texDims.y;
	
	return log2(gamma / radiansPerTexel);
}


#endif //TEXTURE_LOD_HELPERS