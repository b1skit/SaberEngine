// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_COLOR

#include "SaberCommon.hlsli"


float4 PShader(VertexOut In) : SV_Target
{
	return MatAlbedo.Sample(Wrap_Linear_Linear, In.UV0) * In.Color;
}