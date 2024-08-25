// © 2023 Adam Badke. All rights reserved.
#define VOUT_LOCAL_POS

#include "SaberCommon.hlsli"
#include "../Generated/HLSL/VertexStreams_PositionOnly.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	Out.LocalPos = In.Position; // Local vertex position
	
	// Projected position:
	const float3x3 viewRotation =
	{
		CameraParams.g_view[0].xyz,
		CameraParams.g_view[1].xyz,
		CameraParams.g_view[2].xyz
	};
	const float3 rotatedPos = mul(viewRotation, In.Position.xyz);
	
	Out.Position = mul(CameraParams.g_projection, float4(rotatedPos, 1.f));
	
	return Out;
}