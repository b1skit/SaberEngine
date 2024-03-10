// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_TRANSFORMATIONS
#define SABER_TRANSFORMATIONS


vec3 GetWorldPos(vec2 screenUV, float nonLinearDepth, mat4 invViewProjection)
{
	vec2 ndcXY = (screenUV * 2.f) - vec2(1.f, 1.f); // [0,1] -> [-1, 1]

	// Flip the Y coordinate so we can get back to the NDC that GLSL expects.
	// OpenGL uses a RHCS in view space, but LHCS in NDC. Flipping the Y coordinate here effectively reverses the Z axis
	// to account for this change of handedness.
	ndcXY.y *= -1;

	const vec4 ndcPos = vec4(ndcXY.xy, nonLinearDepth, 1.f);

	vec4 result = invViewProjection * ndcPos;
	return result.xyz / result.w; // Apply the perspective division
}


// Convert a linear depth in [near, far] (eye space) to a non-linear depth buffer value in [0,1]
float ConvertLinearDepthToNonLinear(const float near, const float far, const float depthLinear)
{
	const float depthNDC = (((2.0 * near * far) / depthLinear) - far - near) / (near - far);
	const float depthNonLinear = (depthNDC + 1.0) / 2.0;

	return depthNonLinear;
}


// Convert a non-linear depth in [0, 1] (e.g. from a depth buffer) to eye-space depth in [near, far]
// Note: Even in a RHCS looking down -Z, the eye-space [near, far] depth is positive. As demonstrated in this plot,
// negating the near/far values will result in a negative eye-space Z: https://www.desmos.com/calculator/urp2wvvmfm
float ConvertNonLinearDepthToLinear(float near, float far, float nonLinearDepth)
{
	return far * near / (far - nonLinearDepth * (far - near));
}


mat3 BuildLookAtMatrix(vec3 forward)
{
	const vec3 arbitraryUp = abs(forward.z) < 0.999f ? vec3(0, 0, 1) : vec3(1, 0, 0);
	
	const vec3 right = normalize(cross(arbitraryUp, forward));
	
	const vec3 up = normalize(cross(forward, right));
	
	return mat3(right, up, forward);
}


#endif // SABER_TRANSFORMATIONS