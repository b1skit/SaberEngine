// © 2023 Adam Badke. All rights reserved.

#include "SaberCommon.hlsli"


VertexToGeometry VShader(VertexIn In)
{
	VertexToGeometry Out;

	const float4 worldPos = mul(InstancedMeshParams[In.InstanceID].g_model, float4(In.Position, 1.0f));
	Out.Position = worldPos;
	
	return Out;
}