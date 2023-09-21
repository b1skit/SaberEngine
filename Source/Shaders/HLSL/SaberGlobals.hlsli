// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_GLOBALS_HLSL
#define SABER_GLOBALS_HLSL
#include "MathConstants.hlsli"


float3 sRGBToLinear(float3 srgbColor)
{
	// https://en.wikipedia.org/wiki/SRGB#Computing_the_transfer_function
	return srgbColor <= 0.04045 ? (srgbColor / 12.92f) : pow((srgbColor + 0.055f) / 1.055f, 2.4f);
}


float4 sRGBToLinear(float4 srgbColorWithAlpha)
{
	return float4(sRGBToLinear(srgbColorWithAlpha.rgb), srgbColorWithAlpha.a);
}


float3 LinearToSRGB(float3 linearColor)
{
	// https://en.wikipedia.org/wiki/SRGB#Computing_the_transfer_function
	// Note: The 2 functions intersect at x = 0.0031308
	const float3 result =
		linearColor <= 0.0031308 ? 12.92f * linearColor : 1.055f * pow(abs(linearColor), 1.f / 2.4f) - 0.055f;
	
	return result;
}


float4 LinearToSRGB(float4 linearColorWithAlpha)
{
	return float4(LinearToSRGB(linearColorWithAlpha.rgb), linearColorWithAlpha.a);
}


#endif // SABER_GLOBALS_HLSL