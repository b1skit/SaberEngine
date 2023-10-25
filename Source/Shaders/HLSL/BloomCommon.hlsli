// © 2023 Adam Badke. All rights reserved.
#ifndef BLOOM_COMMON
#define BLOOM_COMMON
#include "Color.hlsli"

float ComputeKarisAverageWeight(float3 linearColor)
{
	const float3 sRGBColor = LinearToSRGB(linearColor);
	const float luminance = sRGBToLuminance(sRGBColor);
	const float weight = 1.f / (1.f + luminance);
	return weight;
}


#endif