// © 2023 Adam Badke. All rights reserved.
#ifndef COLOR_HLSL
#define COLOR_HLSL
#include "MathConstants.hlsli"


float3 sRGBToLinear(float3 srgbColor)
{
	// https://en.wikipedia.org/wiki/SRGB#Computing_the_transfer_function
	return select(srgbColor <= 0.04045, (srgbColor / 12.92f), pow((srgbColor + 0.055f) / 1.055f, 2.4f));
}


float4 sRGBToLinear(float4 srgbColorWithAlpha)
{
	return float4(sRGBToLinear(srgbColorWithAlpha.rgb), srgbColorWithAlpha.a);
}


float3 LinearToSRGB(float3 linearColor)
{
	// https://en.wikipedia.org/wiki/SRGB#Computing_the_transfer_function
	// Note: The 2 functions intersect at x = 0.0031308
	return select(linearColor <= 0.0031308, 12.92f * linearColor, 1.055f * pow(abs(linearColor), 1.f / 2.4f) - 0.055f);
}


float4 LinearToSRGB(float4 linearColorWithAlpha)
{
	return float4(LinearToSRGB(linearColorWithAlpha.rgb), linearColorWithAlpha.a);
}


float LinearToLuminance(float3 linearColor)
{
	// https://en.wikipedia.org/wiki/Luma_(video)
	return dot(linearColor, float3(0.2126f, 0.7152f, 0.0722f));
}


float sRGBToLuminance(float3 sRGB)
{	
	return LinearToLuminance(sRGBToLinear(sRGB));
}


#endif // COLOR_HLSL