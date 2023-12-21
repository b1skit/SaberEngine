// © 2023 Adam Badke. All rights reserved.
#define VIN_NORMAL
#define VIN_TANGENT
#define VIN_UV0
#define VIN_COLOR
#define VOUT_UV0
#define VOUT_TBN
#define VOUT_COLOR
#include "NormalMapUtils.hlsli"
#include "SaberCommon.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const float4 worldPos = mul(InstancedTransformParams[In.InstanceID].g_model, float4(In.Position, 1.0f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	Out.UV0 = In.UV0;
	Out.Color = PBRMetallicRoughnessParams.g_baseColorFactor * In.Color;
	
	Out.TBN = BuildTBN(In.Normal, In.Tangent, InstancedTransformParams[In.InstanceID].g_transposeInvModel);
	
	return Out;	
}