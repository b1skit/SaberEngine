// � 2023 Adam Badke. All rights reserved.

#include "SaberCommon.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const uint transformIdx = InstanceIndexParams[In.InstanceID].g_transformIdx;
	
	const float4 worldPos = mul(InstancedTransformParams[transformIdx].g_model, float4(In.Position, 1.0f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	return Out;
}