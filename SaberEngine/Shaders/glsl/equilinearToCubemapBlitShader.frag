#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


#ifdef BLIT_IEM

uniform int numSamples;

// Remap from equirectangular to cubemap, performing IEM filtering (ie. for diffuse IBL)
void main()
{	
	// Direction from the center of the cube map towards the current pixel, in "world" space:
	vec3 worldDir   = normalize(data.localPos);

	// Create an orthonormal basis, with worldDir as our "MatNormal"/up:
	vec3 tangent    = normalize(vec3(worldDir.y + 1.0, worldDir.z, worldDir.x)); // Arbitrary: Ensure we don't end up with cross(worldDir, worldDir)
	vec3 bitangent  = normalize(cross(tangent, worldDir));
	tangent         = normalize(cross(worldDir, bitangent));

	vec3 irradiance = vec3(0.0);
	
	// Hammerseley cosine-weighted sampling:
	for (int i = 0; i < numSamples; i++)
	{
		vec2 samplePoints = Hammersley2D(i, numSamples);

		vec3 hemSample = HemisphereSample_cosineDist(samplePoints.x, samplePoints.y); // TODO: MAKE ARG TAKE A VEC2!!!!

		// Project: Tangent space (Z-up) -> World space:
		hemSample = vec3(
			dot(hemSample, vec3(tangent.x, bitangent.x, worldDir.x)), 
			dot(hemSample, vec3(tangent.y, bitangent.y, worldDir.y)), 
			dot(hemSample, vec3(tangent.z, bitangent.z, worldDir.z)));

		// Sample the environment:
		vec2 equirectangularUVs	= WorldDirToSphericalUV(hemSample);
		irradiance				+= texture(MatAlbedo, equirectangularUVs).rgb;
	}

	// Simple Monte Carlo approximation of the integral:
	irradiance = irradiance / float(numSamples); // TODO: Should this be  M_PI * irradiance / float(numSamples); ??

	FragColor = vec4(irradiance, 1.0);
}


#elif defined BLIT_PMREM

uniform int		numSamples;
uniform float	roughness;

// Remap from equirectangular to cubemap, performing PMREM filtering (ie. for specular IBL)
void main()
{
	vec3 N = normalize(data.localPos);    
	vec3 R = N;
	vec3 V = R;		

	float totalWeight = 0.0;
	vec3 sampledColor = vec3(0.0);     
	for(int i = 0; i < numSamples; i++)
	{
		vec2 Xi = Hammersley2D(i, numSamples);
		vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
		vec3 L  = normalize(2.0 * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0.0);
		if(NdotL > 0.0)
		{
			vec2 equirectangularUVs	= WorldDirToSphericalUV(L);

			sampledColor += texture(MatAlbedo, equirectangularUVs).rgb;

			totalWeight += NdotL;
		}
	}
	sampledColor = sampledColor / totalWeight;

	FragColor = vec4(sampledColor, 1.0);
}


#else

void main() // Deprecated: Just show an error color to be safe
{	
	FragColor = vec4(1.0, 0, 0, 1.0);
}

#endif