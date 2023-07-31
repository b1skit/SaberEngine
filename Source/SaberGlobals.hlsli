// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_GLOBALS_HLSL
#define SABER_GLOBALS_HLSL


float4 sRGBToLinear(float4 srgbColor)
{
	// https://en.wikipedia.org/wiki/SRGB#Computing_the_transfer_function
	return srgbColor <= 0.04045 ? (srgbColor / 12.92f) : pow((srgbColor + 0.055f) / 1.055f, 2.4f);
}


float4 LinearToSRGB(float4 linearColor)
{
	// https://en.wikipedia.org/wiki/SRGB#Computing_the_transfer_function
	const float4 result =
		linearColor <= 0.0031308 ? 12.92f * linearColor : 1.055f * pow(max(0.f, linearColor), 1.f / 2.4f) - 0.055f;
	return max(result, 0.f);
}


float2 PixelCoordsToUV(uint2 pixelCoords, float2 texWidthHeight)
{
	return (float2(pixelCoords) + float2(0.5f, 0.5f)) / texWidthHeight;
}


#endif // SABER_GLOBALS_HLSL