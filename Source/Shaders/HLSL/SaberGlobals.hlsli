// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_GLOBALS_HLSL
#define SABER_GLOBALS_HLSL

// Global defines:
//----------------
#define M_PI		3.1415926535897932384626433832795	// pi
#define M_2PI       6.28318530717958647693		// 2pi
#define M_4PI       12.5663706143591729539		// 4pi
#define M_PI_2      1.57079632679489661923		// pi/2
#define M_PI_4      0.785398163397448309616		// pi/4
#define M_1_PI      0.318309886183790671538		// 1/pi
#define M_2_PI      0.636619772367581343076		// 2/pi
#define M_4_PI      1.27323954473516268615		// 4/pi
#define M_1_2PI     0.159154943091895335769		// 1/(2pi)
#define M_1_4PI     0.079577471545947667884		// 1/(4pi)
#define M_SQRTPI    1.77245385090551602730		// sqrt(pi)
#define M_2_SQRTPI  1.12837916709551257390		// 2/sqrt(pi)
#define M_SQRT2     1.41421356237309504880		// sqrt(2)
#define M_1_SQRT2   0.707106781186547524401		// 1/sqrt(2)

#define FLT_MAX		3.402823466e+38
#define FLT_MIN		1.175494351e-38


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


float2 PixelCoordsToUV(uint2 pixelCoords, uint2 texWidthHeight, float2 offset = float2(0.5f, 0.5f))
{
	return (float2(pixelCoords) + offset) / texWidthHeight;
}


#endif // SABER_GLOBALS_HLSL