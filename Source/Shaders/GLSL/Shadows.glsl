// © 2024 Adam Badke. All rights reserved.
#ifndef SHADOWS_COMMON
#define SHADOWS_COMMON


void GetBiasedShadowWorldPos(
	vec3 worldPos, vec3 worldNormal, vec3 lightWDir, vec2 minMaxShadowBias, out vec3 shadowWorldPosOut)
{
	const float NoL = clamp(dot(worldNormal, lightWDir), 0.f, 1.f);
	
	shadowWorldPosOut = worldPos + (worldNormal * max(minMaxShadowBias.x, minMaxShadowBias.y * (1.f - NoL)));
}


float Get2DShadowFactor(
	vec3 worldPos,
	vec3 worldNormal,
	vec3 lightWorldDir,
	mat4 shadowCamVP, 
	vec2 minMaxShadowBias,
	vec2 invShadowMapWidthHeight)
{
	vec3 biasedShadowWPos;
	GetBiasedShadowWorldPos(worldPos, worldNormal, lightWorldDir, minMaxShadowBias, biasedShadowWPos);

	// Transform our worldPos to shadow map projection space:
	vec4 shadowProjPos = shadowCamVP * vec4(biasedShadowWPos, 1.f);
	shadowProjPos /= shadowProjPos.w; // Not necessary for orthogonal matrices

	// Shadow map UVs:
	vec2 shadowmapUVs = (shadowProjPos.xy + 1.f) * 0.5f;
	shadowmapUVs.y = 1.f - shadowmapUVs.y; // Clip space Y+ is up, UV Y+ is down, so we flip Y here

	// Compute a block of samples around our fragment, starting at the top-left. Note: MUST be a power of two.
	// TODO: Compute this on C++ side and allow for uploading of arbitrary samples (eg. odd, even)
	const int gridSize = 4; 

	const float offsetMultiplier = (float(gridSize) / 2.f) - 0.5;

	shadowmapUVs.x -= offsetMultiplier * g_shadowMapTexelSize.z;
	shadowmapUVs.y += offsetMultiplier * g_shadowMapTexelSize.w;

	float depthSum = 0;
	for (int row = 0; row < gridSize; row++)
	{
		for (int col = 0; col < gridSize; col++)
		{
			depthSum += texture(Depth0, vec3(shadowmapUVs, shadowProjPos.z)).r;

			shadowmapUVs.x += g_shadowMapTexelSize.z;
		}

		shadowmapUVs.x -= gridSize * g_shadowMapTexelSize.z;
		shadowmapUVs.y -= g_shadowMapTexelSize.w;
	}

	depthSum /= (gridSize * gridSize);

	return depthSum;
}


// Compute a soft shadow factor from a cube map
// Based on Lengyel's Foundations of Game Engine Development Volume 2: Rendering, p164, listing 8.8
float GetCubeShadowFactor(
	vec3 worldPos,
	vec3 worldNormal,
	vec3 lightWorldPos,
	vec3 lightWorldDir,
	vec2 shadowCamNearFar, 
	vec2 minMaxShadowBias,
	float cubeFaceDim)
{
	vec3 biasedShadowWPos;
	GetBiasedShadowWorldPos(worldPos, worldNormal, lightWorldDir, minMaxShadowBias, biasedShadowWPos);

	const vec3 lightToPoint = biasedShadowWPos - lightWorldPos;
	vec3 cubeSampleDir = WorldToCubeSampleDir(lightToPoint);

	// Calculate non-linear, projected depth buffer depth from the light-to-fragment direction. The eye depth w.r.t
	// our cubemap view is the value of the largest component of this direction. Also apply a slope-scale bias.
	const vec3 absSampleDir = abs(cubeSampleDir);
	const float maxXY = max(absSampleDir.x, absSampleDir.y);
	const float eyeDepth = max(maxXY, absSampleDir.z);

	const float nonLinearDepth = 
		ConvertLinearDepthToNonLinear(g_shadowCamNearFarBiasMinMax.x, g_shadowCamNearFarBiasMinMax.y, eyeDepth);

	// Compute a sample offset for PCF shadow samples:
	const float sampleOffset = 2.f / cubeFaceDim;

	// Calculate offset vectors:
	float offset = sampleOffset * eyeDepth;
	float dxy = (maxXY > absSampleDir.z) ? offset : 0.f;
	float dx = (absSampleDir.x > absSampleDir.y) ? dxy : 0.f;
	vec2 oxy = vec2(offset - dx, dx);
	vec2 oyz = vec2(offset - dxy, dxy);

	vec3 limit = vec3(eyeDepth, eyeDepth, eyeDepth);
	const float bias = 1.f / 1024.0; // Epsilon = 1/1024

	limit.xy -= oxy * bias;
	limit.yz -= oyz * bias;

	// Get the center sample:
	float light = texture(CubeDepth, vec4(cubeSampleDir.xyz, nonLinearDepth)).r;

	// Get 4 extra samples at diagonal offsets:
	cubeSampleDir.xy -= oxy;
	cubeSampleDir.yz -= oyz;

	light += texture(CubeDepth, vec4(clamp(cubeSampleDir, -limit, limit), nonLinearDepth)).r;
	cubeSampleDir.xy += oxy * 2.f;

	light += texture(CubeDepth, vec4(clamp(cubeSampleDir, -limit, limit), nonLinearDepth)).r;
	cubeSampleDir.yz += oyz * 2.f;

	light += texture(CubeDepth, vec4(clamp(cubeSampleDir, -limit, limit), nonLinearDepth)).r;
	cubeSampleDir.xy -= oxy * 2.f;

	light += texture(CubeDepth, vec4(clamp(cubeSampleDir, -limit, limit), nonLinearDepth)).r;

	return (light * 0.2);	// Return the average of our 5 samples
}

#endif