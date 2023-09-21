// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "SaberCommon.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	Out.Position = float4(In.Position, 1.f); // Our screen aligned quad is already in clip space
	Out.UV0 = In.UV0;
	
	return Out;
}