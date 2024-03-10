// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_UVUTILS
#define SABER_UVUTILS

#include "MathConstants.glsl"


// Transform pixel coordintes ([0, xRes), [0, yRes)) to [0,1] screen-space UVs.
//	offset: Use this if your pixel coords are centered in the pixel. NOTE: By default, OpenGL pixel centers (e.g. as 
//		found in gl_FragCoord) are located at half-pixel centers (e.g. (0.5, 0.5) is the top-left pixel in SaberEngine). 
//		Thus if using gl_FragCoord, you'll likely want a vec2(0,0) offset
//	doFlip: Use true if reading from a flipped FBO (e.g. GBuffer), false if the image being sampled has the correct
//		orientation
vec2 PixelCoordsToUV(vec2 pixelXY, vec2 screenWidthHeight, vec2 offset = vec2(0.5f, 0.5f), bool doFlip = true)
{
	vec2 screenUV = (vec2(pixelXY) + offset) / screenWidthHeight;
	if (doFlip)
	{
		screenUV.y = 1.f - screenUV.y;
	}
	return screenUV;
}


// Convert a world-space direction to spherical coordinates (i.e. latitude/longitude map UVs in [0, 1])
// The center of the texture is at -Z, with the left and right edges meeting at +Z.
// i.e. dir(0, 0, -1) = UV(0.5, 0.5)
vec2 WorldDirToSphericalUV(vec3 unnormalizedDirection)
{
	const vec3 dir = normalize(unnormalizedDirection);

	vec2 uv;

	// Note: Reverse atan variables to change env. map orientation about y
	uv.x = atan(dir.x, -dir.z) * M_1_2PI + 0.5f; // atan == atan2 in HLSL

	uv.y = acos(dir.y) * M_1_PI; // Note: Use -p.y for (0,0) bottom left UVs, +p.y for (0,0) top left UVs

	return uv;
}


// Convert spherical map [0,1] UVs to a normalized world-space direction.
// Note: 0 rotation about Y == Z+ == UV [0, y] / [1, y]
// UV [0.5, 0.5] (i.e. middle of the spherical map) = Z-
vec3 SphericalUVToWorldDir(vec2 uv)
{
	// Note: Currently untested in GLSL, but known good in HLSL
	const float phi	= M_PI * uv.y;
	const float theta = M_2PI - (M_2PI * uv.x);

	float sinPhi = sin(phi);

	vec3 dir;
	dir.x = sinPhi * sin(theta);
	dir.y = cos(phi);
	dir.z = sinPhi * cos(theta);

	return normalize(dir);
}


// Converts a RHCS world-space direction to a LHCS cubemap sample direction.
vec3 WorldToCubeSampleDir(vec3 worldDir)
{
	return vec3(worldDir.x, worldDir.y, -worldDir.z);
}

#endif // SABER_UVUTILS