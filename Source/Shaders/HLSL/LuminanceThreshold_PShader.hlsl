// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "SaberCommon.hlsli"


float4 PShader(VertexOut In) : SV_Target
{
	// TODO: Populate this correctly:
	return float4(In.UV0.xy, 0.f, 1.f);
}