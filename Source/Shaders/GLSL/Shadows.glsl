// © 2024 Adam Badke. All rights reserved.
#ifndef SHADOWS_COMMON
#define SHADOWS_COMMON


// Compute a depth map bias value based on surface orientation
float GetSlopeScaleBias(float NoL, vec2 minMaxShadowBias)
{
	return max(minMaxShadowBias.x, minMaxShadowBias.y * (1.f - NoL));
}


// Find out if a fragment (in world space) is in shadow
float GetShadowFactor(vec3 shadowPos, sampler2DShadow shadowMap, float NoL)
{
	// Convert Projection [-1, 1], -> Screen/UV [0,1] space.
	// Note: SaberEngine overrides the default OpenGL coordinate system (via glClipControl/GLM_FORCE_DEPTH_ZERO_TO_ONE),
	// so z is already in [0,1]
	vec3 shadowScreen = vec3((shadowPos.xy + 1.f) / 2.f, shadowPos.z); 
	shadowScreen.y = 1.f - shadowScreen.y; // UV (0,0) is in the top-left

	// Compute a slope-scaled bias depth:
	const float biasedDepth = shadowScreen.z - GetSlopeScaleBias(NoL, g_shadowCamNearFarBiasMinMax.zw);

	// Compute a block of samples around our fragment, starting at the top-left. Note: MUST be a power of two.
	// TODO: Compute this on C++ side and allow for uploading of arbitrary samples (eg. odd, even)
	const int gridSize = 4; 

	const float offsetMultiplier = (float(gridSize) / 2.f) - 0.5;

	shadowScreen.x -= offsetMultiplier * g_shadowMapTexelSize.z;
	shadowScreen.y += offsetMultiplier * g_shadowMapTexelSize.w;

	float depthSum = 0;
	for (int row = 0; row < gridSize; row++)
	{
		for (int col = 0; col < gridSize; col++)
		{
			depthSum += texture(shadowMap, vec3(shadowScreen.xy, biasedDepth)).r;

			shadowScreen.x += g_shadowMapTexelSize.z;
		}

		shadowScreen.x -= gridSize * g_shadowMapTexelSize.z;
		shadowScreen.y -= g_shadowMapTexelSize.w;
	}

	depthSum /= (gridSize * gridSize);

	return depthSum;
}


// Compute a soft shadow factor from a cube map.
// Based on Lengyel's Foundations of Game Engine Development Volume 2: Rendering, p164, listing 8.8
float GetShadowFactor(vec3 lightToFrag, samplerCubeShadow shadowMap, const float NoL)
{
	vec3 sampleDir = WorldToCubeSampleDir(lightToFrag);

	const float cubemapFaceResolution = g_shadowMapTexelSize.x; // Assume our shadow cubemap has square faces...	

	// Calculate non-linear, projected depth buffer depth from the light-to-fragment direction. The eye depth w.r.t
	// our cubemap view is the value of the largest component of this direction. Also apply a slope-scale bias.
	const vec3 absSampleDir = abs(sampleDir);
	const float maxXY = max(absSampleDir.x, absSampleDir.y);
	const float eyeDepth = max(maxXY, absSampleDir.z);
	const float biasedEyeDepth = eyeDepth - GetSlopeScaleBias(NoL, g_shadowCamNearFarBiasMinMax.zw);

	const float nonLinearDepth = 
		ConvertLinearDepthToNonLinear(g_shadowCamNearFarBiasMinMax.x, g_shadowCamNearFarBiasMinMax.y, biasedEyeDepth);

	// Compute a sample offset for PCF shadow samples:
	const float sampleOffset = 2.f / cubemapFaceResolution;

	// Calculate offset vectors:
	float offset = sampleOffset * eyeDepth;
	float dxy = (maxXY > absSampleDir.z) ? offset : 0.f;
	float dx = (absSampleDir.x > absSampleDir.y) ? dxy : 0.f;
	vec2 oxy = vec2(offset - dx, dx);
	vec2 oyz = vec2(offset - dxy, dxy);

	vec3 limit = vec3(eyeDepth, eyeDepth, eyeDepth);
	const float bias = 1.f / 1024.0; // Epsilon = 1/1024.

	limit.xy -= oxy * bias;
	limit.yz -= oyz * bias;

	// Get the center sample:
	float light = texture(shadowMap, vec4(sampleDir.xyz, nonLinearDepth)).r;

	// Get 4 extra samples at diagonal offsets:
	sampleDir.xy -= oxy;
	sampleDir.yz -= oyz;

	light += texture(shadowMap, vec4(clamp(sampleDir, -limit, limit), nonLinearDepth)).r;
	sampleDir.xy += oxy * 2.f;

	light += texture(shadowMap, vec4(clamp(sampleDir, -limit, limit), nonLinearDepth)).r;
	sampleDir.yz += oyz * 2.f;

	light += texture(shadowMap, vec4(clamp(sampleDir, -limit, limit), nonLinearDepth)).r;
	sampleDir.xy -= oxy * 2.f;

	light += texture(shadowMap, vec4(clamp(sampleDir, -limit, limit), nonLinearDepth)).r;

	return (light * 0.2);	// Return the average of our 5 samples
}

#endif