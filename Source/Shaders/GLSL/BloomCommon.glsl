// � 2023 Adam Badke. All rights reserved.
#version 460
#ifndef BLOOM_COMMON
#define BLOOM_COMMON
#include "Color.glsl"


float ComputeKarisAverageWeight(vec3 linearColor)
{
	const vec3 sRGBColor = LinearToSRGB(linearColor);
	const float luminance = sRGBToLuminance(sRGBColor);
	const float weight = 1.f / (1.f + luminance);
	return weight;
}


#endif