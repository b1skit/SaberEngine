// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	// TODO: Populate these correctly:
	Out.Position = float4(In.Position, 1.f);
	Out.UV0 = In.UV0;
	Out.Color = In.Color;
	
	return Out;
}