// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"


float4 PShader(VertexOut In) : SV_Target
{
	return In.Color;
}