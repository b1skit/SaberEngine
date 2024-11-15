// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "SaberCommon.hlsli"

Texture2D<float4> Tex0;


float4 PShader(VertexOut In) : SV_Target
{
	return Tex0.Sample(ClampMinMagLinearMipPoint, In.UV0.xy);
}