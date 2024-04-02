// © 2023 Adam Badke. All rights reserved.

#include "SaberCommon.hlsli"


VertexToGeometry VShader(VertexIn In)
{
	VertexToGeometry Out;
	
	const uint transformIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_transformIdx;

	const float4 worldPos = mul(InstancedTransformParams[transformIdx].g_model, float4(In.Position, 1.0f));
	Out.Position = worldPos;
	
	return Out;
}