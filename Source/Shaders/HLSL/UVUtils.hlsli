// � 2023 Adam Badke. All rights reserved.
#ifndef SABER_UVUTILS
#define SABER_UVUTILS

#include "MathConstants.hlsli"


// Note: In DX12, SV_POSITION is automatically offset by 0.5 (e.g. the top-left pixel will have an SV_POSITION of
// (0.5, 0.5) by default). More info here: https://www.asawicki.info/news_1516_half-pixel_offset_in_directx_11
// Supply an offset of (0.5, 0.5) here when you have non-offset coordinates (i.e. top-left = (0, 0))
float2 PixelCoordsToUV(uint2 pixelCoords, uint2 texWidthHeight, float2 offset)
{
	return (float2(pixelCoords) + offset) / texWidthHeight;
}


float2 WorldDirToSphericalUV(float3 unnormalizedDirection)
{
	const float3 dir = normalize(unnormalizedDirection);

	// Note: Reverse atan2 variables to change env. map orientation about y
	float2 uv = float2(
		atan2(dir.x, -dir.z) * M_1_2PI + 0.5f,
		acos(dir.y) * M_1_PI); // Note: Use -dir.y for (0,0) bottom left UVs, +dir.y for (0,0) top left UVs

	return uv;
}


#endif // SABER_UVUTILS