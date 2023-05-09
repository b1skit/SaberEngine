// © 2022 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const float4 worldPos = mul(InstancedMeshParams[In.InstanceID].g_model, float4(In.Position, 1.0f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	// TEMP HAX: PBRMetallicRoughnessParams is not initialized, force a non-zero value
	Out.Color = max(float4(1.f, 1.f, 1.f, 1.f), PBRMetallicRoughnessParams.g_baseColorFactor) * In.Color;
	
	return Out;
}