// © 2023 Adam Badke. All rights reserved.
#define VIN_NORMAL
#define VIN_TANGENT
#define VIN_UV0
#define VIN_COLOR
#define VOUT_UV0
#define VOUT_TBN
#define VOUT_COLOR
#define VOUT_INSTANCE_ID
#include "NormalMapUtils.hlsli"
#include "SaberCommon.hlsli"


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const uint transformIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_transformIdx;
	const uint materialIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_materialIdx;
	
	const float4 worldPos = mul(InstancedTransformParams[transformIdx].g_model, float4(In.Position, 1.0f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
#if defined(VOUT_WORLD_POS)
	Out.WorldPos = worldPos.xyz;
#endif
	
	Out.UV0 = In.UV0;

	Out.Color = InstancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor * In.Color;
	
	Out.TBN = BuildTBN(In.Normal, In.Tangent, InstancedTransformParams[transformIdx].g_transposeInvModel);
	
	Out.InstanceID = In.InstanceID;
	
	return Out;	
}