// © 2023 Adam Badke. All rights reserved.
#ifndef COLOR_GLSL
#define COLOR_GLSL
#include "MathConstants.glsl"


vec3 sRGBToLinear(vec3 srgbColor)
{
	// https://en.wikipedia.org/wiki/SRGB#Computing_the_transfer_function
	return vec3(
		srgbColor.r <= 0.04045 ? (srgbColor.r / 12.92f) : pow((srgbColor.r + 0.055f) / 1.055f, 2.4f),
		srgbColor.g <= 0.04045 ? (srgbColor.g / 12.92f) : pow((srgbColor.g + 0.055f) / 1.055f, 2.4f),
		srgbColor.b <= 0.04045 ? (srgbColor.b / 12.92f) : pow((srgbColor.b + 0.055f) / 1.055f, 2.4f));
}


vec4 sRGBToLinear(vec4 srgbColorWithAlpha)
{
	return vec4(sRGBToLinear(srgbColorWithAlpha.rgb), srgbColorWithAlpha.a);
}


vec3 LinearToSRGB(vec3 linearColor)
{
	// https://en.wikipedia.org/wiki/SRGB#Computing_the_transfer_function
	// Note: The 2 functions intersect at x = 0.0031308

	return vec3(
		linearColor.r <= 0.0031308 ? 12.92f * linearColor.r : 1.055f * pow(abs(linearColor.r), 1.f / 2.4f) - 0.055f,
		linearColor.g <= 0.0031308 ? 12.92f * linearColor.g : 1.055f * pow(abs(linearColor.g), 1.f / 2.4f) - 0.055f,
		linearColor.b <= 0.0031308 ? 12.92f * linearColor.b : 1.055f * pow(abs(linearColor.b), 1.f / 2.4f) - 0.055f);
}


vec4 LinearToSRGB(vec4 linearColorWithAlpha)
{
	return vec4(LinearToSRGB(linearColorWithAlpha.rgb), linearColorWithAlpha.a);
}


float sRGBToLuminance(vec3 sRGB)
{	
	return dot(sRGB, vec3(0.2126f, 0.7152f, 0.0722f));
}


#endif // COLOR_GLSL