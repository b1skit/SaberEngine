// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_TRANSFORMATIONS
#define SABER_TRANSFORMATIONS


float3 GetWorldPos(float2 screenUV, float nonLinearDepth, float4x4 invViewProjection)
{
	float2 ndcXY = (screenUV * 2.f) - float2(1.f, 1.f); // [0,1] -> [-1, 1]

	// In SaberEngine, the (0, 0) UV origin is in the top-left, which means +Y is down in UV space.
	// In NDC, +Y is up and point (-1, -1) is in the bottom left.
	// Thus we must flip the Y coordinate here to compensate.
	ndcXY.y *= -1;

	const float4 ndcPos = float4(ndcXY.xy, nonLinearDepth, 1.f);

	float4 result = mul(invViewProjection, ndcPos);
	return result.xyz / result.w; // Apply the perspective division
}


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


#endif // SABER_TRANSFORMATIONS