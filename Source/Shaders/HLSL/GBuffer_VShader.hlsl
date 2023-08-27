// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN
#include "SaberCommon.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const float4 worldPos = mul(InstancedMeshParams[In.InstanceID].g_model, float4(In.Position, 1.0f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	Out.UV0 = In.UV0;
	Out.Color = PBRMetallicRoughnessParams.g_baseColorFactor * In.Color;
	
	// TODO: HLSL does not provide a matrix inverse function
	Out.TBN = float3x3(
		1.f,	0.f,	0.f,
		0.f,	1.f,	0.f,
		0.f,	0.f,	1.f);
	
	return Out;	
}