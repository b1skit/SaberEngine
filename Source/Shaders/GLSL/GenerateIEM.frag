#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


// Remap from equirectangular to cubemap, performing IEM filtering (ie. for diffuse IBL)
void main()
{	
	// Direction from the center of the cube map towards the current pixel, in "world" space:
	vec3 worldDir   = normalize(vOut.localPos);

	// Create an orthonormal basis, with worldDir as our "MatNormal"/up:
	vec3 tangent = normalize(vec3(worldDir.y + 1.0, worldDir.z, worldDir.x)); // Arbitrary: Ensure we don't end up with cross(worldDir, worldDir)
	vec3 bitangent = normalize(cross(tangent, worldDir));
	tangent = normalize(cross(worldDir, bitangent));

	vec3 irradiance = vec3(0.0);
	
	// Hammerseley cosine-weighted sampling:
	const int numSamples = int(g_numSamplesRoughness.x); // .x = numIEMSamples, .y = numPMREMSamples, .z = roughness
	for (int i = 0; i < numSamples; i++)
	{
		vec2 samplePoints = Hammersley2D(i, numSamples);

		vec3 hemSample = HemisphereSample_cosineDist(samplePoints.x, samplePoints.y); // TODO: Make input arg a vec2

		// Project: Tangent space (Z-up) -> World space:
		hemSample = vec3(
			dot(hemSample, vec3(tangent.x, bitangent.x, worldDir.x)), 
			dot(hemSample, vec3(tangent.y, bitangent.y, worldDir.y)), 
			dot(hemSample, vec3(tangent.z, bitangent.z, worldDir.z)));

		// Sample the environment:
		vec2 equirectangularUVs	= WorldDirToSphericalUV(hemSample);
		irradiance += texture(Tex0, equirectangularUVs).rgb;
	}

	// Simple Monte Carlo approximation of the integral:
	irradiance = irradiance / float(numSamples); // TODO: Should this be  M_PI * irradiance / float(numSamples); ??

	FragColor = vec4(irradiance, 1.0);
}