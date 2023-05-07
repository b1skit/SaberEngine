// © 2022 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"


float4 PShader(PixelIn In) : SV_Target
{
	return In.Color;
}