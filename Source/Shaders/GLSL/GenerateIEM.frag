#version 460

#define SABER_VEC4_OUTPUT
#define VOUT_LOCAL_POS

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


// Remap from equirectangular to cubemap, performing IEM filtering (ie. for diffuse IBL)
void main()
{	
	// World-space direction from the center of the cube towards the current cubemap pixel
	const vec3 pxWorldDir = normalize(vOut.localPos);

	// Create an orthonormal basis, with pxWorldDir as our up vector.
	// Add some values to the tangent to ensure we don't end up with cross(pxWorldDir, pxWorldDir)
	const vec3 arbitraryVector = normalize(vec3(pxWorldDir.y + 1.0, pxWorldDir.z, pxWorldDir.x));
	const vec3 bitangent = normalize(cross(arbitraryVector, pxWorldDir));
	const vec3 tangent = normalize(cross(pxWorldDir, bitangent));

	vec3 irradiance = vec3(0.0);
	
	const int numSamples = int(g_numSamplesRoughnessFaceIdx.x);
	const float srcMip = g_mipLevel.x;
	for (int i = 0; i < numSamples; i++)
	{
		const vec2 samplePoints = Hammersley2D(i, numSamples);

		vec3 hemSample = HemisphereSample_cosineDist(samplePoints.x, samplePoints.y);

		// Project: Tangent space (Z-up) -> World space:
		hemSample = vec3(
			dot(hemSample, vec3(tangent.x, bitangent.x, pxWorldDir.x)), 
			dot(hemSample, vec3(tangent.y, bitangent.y, pxWorldDir.y)), 
			dot(hemSample, vec3(tangent.z, bitangent.z, pxWorldDir.z)));

		// Sample the environment:
		vec2 equirectangularUVs	= WorldDirToSphericalUV(hemSample);
		irradiance += texture(Tex0, equirectangularUVs, srcMip).rgb;
	}

	// Simple Monte Carlo approximation of the integral:
	irradiance = irradiance / float(numSamples);

	FragColor = vec4(irradiance, 1.0);
}