// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_TBN
#define VOUT_COLOR

#include "NormalMapUtils.hlsli"
#include "SaberCommon.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const float4 worldPos = mul(InstancedMeshParams[In.InstanceID].g_model, float4(In.Position, 1.0f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	Out.UV0 = In.UV0;
	Out.Color = PBRMetallicRoughnessParams.g_baseColorFactor * In.Color;
	
	Out.TBN = BuildTBN(In.Normal, In.Tangent, InstancedMeshParams[In.InstanceID].g_transposeInvModel);
	
	return Out;	
}