// © 2022 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	//const float4 worldPos = mul(InstancedMeshParams[0].g_model, float4(In.Position, 1.0f));
	//Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	//Out.Color = max(float4(1.f, 1.f, 1.f, 1.f), PBRMetallicRoughnessParams.g_baseColorFactor) * In.Color;
	
	Out.Position = mul(CameraParams.g_viewProjection, float4(In.Position, 1.0f));
	Out.Color = In.Color;

	return Out;
}