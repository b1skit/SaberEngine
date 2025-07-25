// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_UVUTILS
#define SABER_UVUTILS

#include "MathConstants.hlsli"


// Note: In DX12, SV_POSITION is automatically offset by 0.5 (e.g. the top-left pixel will have an SV_POSITION of
// (0.5, 0.5) by default). More info here: https://www.asawicki.info/news_1516_half-pixel_offset_in_directx_11
// Supply an offset of (0.5, 0.5) here when you have non-offset coordinates (i.e. top-left = (0, 0))
float2 PixelCoordsToScreenUV(uint2 pixelCoords, uint2 screenWidthHeight, float2 offset)
{
	return (float2(pixelCoords) + offset) / screenWidthHeight;
}


float3 ScreenUVToWorldPos(float2 screenUV, float nonLinearDepth, float4x4 invViewProjection)
{
	float2 ndcXY = (screenUV * 2.f) - float2(1.f, 1.f); // [0,1] -> [-1, 1]

	// In SaberEngine, the (0, 0) UV origin is in the top-left, which means +Y is down in UV space.
	// In NDC, +Y is up and point (-1, -1) is in the bottom left.
	// Thus we must flip the Y coordinate here to compensate.
	ndcXY.y *= -1;

	const float4 ndcPos = float4(ndcXY.xy, nonLinearDepth, 1.f);

	float4 result = mul(invViewProjection, ndcPos);
	return result.xyz / result.w; // Apply the perspective division
}


// Convert a world-space direction to spherical coordinates (i.e. latitude/longitude map UVs in [0, 1])
// The center of the texture is at -Z, with the left and right edges meeting at +Z.
// i.e. dir(0, 0, -1) = UV(0.5, 0.5)
float2 WorldDirToSphericalUV(float3 unnormalizedDirection)
{
	const float3 dir = normalize(unnormalizedDirection);

	// Note: Reverse atan2 variables to change env. map orientation about y
	const float2 uv = float2(
		atan2(dir.x, -dir.z) * M_1_2PI + 0.5f,
		acos(dir.y) * M_1_PI); // Note: Use -dir.y for (0,0) bottom left UVs, +dir.y for (0,0) top left UVs
	
	return uv;
}


// Converts a RHCS world-space direction to a LHCS cubemap sample direction.
float3 WorldToCubeSampleDir(float3 worldDir)
{
	return float3(worldDir.x, worldDir.y, -worldDir.z);
}


#if defined(MAX_UV_CHANNEL_IDX)

float2 GetUV(VertexOut In, uint channelIdx)
{	
#if MAX_UV_CHANNEL_IDX == 0
	return In.UV0;
	
#else
	const uint finalChannelIdx = min(channelIdx, MAX_UV_CHANNEL_IDX);
	
	// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-switch?redirectedfrom=MSDN
	[call]
	switch (finalChannelIdx)
	{
		case 0: return In.UV0;
#if MAX_UV_CHANNEL_IDX >= 1
		case 1: return In.UV1;
#endif
		default: return float2(0.f, 0.f); // Error!
	}

#endif // End of else block

} //GetUV()

#endif // UV_CHANNEL_SELECTION

#endif // SABER_UVUTILS