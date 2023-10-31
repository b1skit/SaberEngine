// © 2023 Adam Badke. All rights reserved.

#define VIN_COLOR
#define VOUT_COLOR
#include "SaberCommon.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const float4 worldPos = mul(InstancedMeshParams[In.InstanceID].g_model, float4(In.Position, 1.0f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	Out.Color = In.Color;
	
	return Out;
}