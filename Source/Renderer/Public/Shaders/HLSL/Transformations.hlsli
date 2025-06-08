// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_TRANSFORMATIONS
#define SABER_TRANSFORMATIONS


// Convert linear depth in view space [near, far] to a non-linear depth buffer value in [0,1]
float ConvertLinearDepthToNonLinear(float near, float far, float depthLinear)
{
	const float depthNDC = (((2.f * near * far) / depthLinear) - far - near) / (near - far);
	const float depthNonLinear = (depthNDC + 1.f) / 2.f;

	return depthNonLinear;
}


// Convert a non-linear depth in [0, 1] (e.g. from a depth buffer) to eye-space depth in [near, far]
// Note: Even in a RHCS looking down -Z, the eye-space [near, far] depth is positive. As demonstrated in this plot,
// negating the near/far values will result in a negative eye-space Z: https://www.desmos.com/calculator/urp2wvvmfm
float ConvertNonLinearDepthToLinear(float near, float far, float nonLinearDepth)
{
	return far * near / (far - nonLinearDepth * (far - near));
}


float3x3 BuildLookAtMatrix(float3 forward)
{
	const float3 arbitraryUp = abs(forward.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
	
	const float3 right = normalize(cross(arbitraryUp, forward));
	
	const float3 up = normalize(cross(forward, right));
	
	return float3x3(
		right.x, up.x, forward.x,
		right.y, up.y, forward.y,
		right.z, up.z, forward.z);
}


float3x3 GetTransposeInvRotationScale(float4x4 transposeInvModel)
{
	// Note: In HLSL matrices declared in a shader body do not get packed into constant registers (which is done with 
	// column major ordering). Row-major/column-major packing order has no influence on the packing order of matrix
	// constructors (which always follow row-major ordering)	
	return float3x3(
		transposeInvModel[0].xyz,
		transposeInvModel[1].xyz,
		transposeInvModel[2].xyz);
}


#endif // SABER_TRANSFORMATIONS