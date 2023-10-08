// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const float4 worldPos = mul(InstancedMeshParams[In.InstanceID].g_model, float4(In.Position, 1.f));
	
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	return Out;
}