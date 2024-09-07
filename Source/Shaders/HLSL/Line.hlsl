// © 2023 Adam Badke. All rights reserved.
#define VOUT_COLOR
#include "SaberCommon.hlsli"
#include "../Generated/HLSL/VertexStreams_PositionColor.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const float4 worldPos = mul(InstancedTransformParams[In.InstanceID].g_model, float4(In.Position, 1.0f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	Out.Color = In.Color;
	
	return Out;
}


float4 PShader(VertexOut In) : SV_Target
{
	return In.Color;
}