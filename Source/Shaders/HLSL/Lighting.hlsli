// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_LIGHTING
#define SABER_LIGHTING


// Compute the dominant direction for sampling a Disney diffuse retro-reflection lobe from the IEM probe. The 
// Based on listing 23 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
float3 GetDiffuseDominantDir(float3 N, float3 V, float NoV, float roughness)
{
	const float a = 1.02341f * roughness - 1.51174f;
	const float b = -0.511705f * roughness + 0.755868f;
	const float lerpFactor = saturate((NoV * a + b) * roughness);
	
	return lerp(N, V, lerpFactor); // Don't normalize as this vector is for sampling a cubemap
}


#endif