// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "SaberCommon.hlsli"


float4 PShader(VertexOut In) : SV_Target
{
	return Tex0.Sample(Clamp_Linear_Linear, In.UV0.xy);
}