// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_TBN
#define VOUT_COLOR
#define SABER_INSTANCING
#include "NormalMapUtils.hlsli"
#include "SaberCommon.hlsli"

#if defined(MORPH_POS8)
#include "../Generated/HLSL/VertexStreams_PosNmlTanUvCol_Morph_Pos8.hlsli"
#else
#include "../Generated/HLSL/VertexStreams_PosNmlTanUvCol.hlsli"
#endif



VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const uint transformIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_transformIdx;
	const uint materialIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_materialIdx;
	
	float3 position = In.Position;
	
	// TODO: Implement this correctly. For now, just prove we're getting the data we need
#if defined(MORPH_POS8)
	position += In.PositionMorph1;
#endif
	
	const float4 worldPos = mul(InstancedTransformParams[transformIdx].g_model, float4(position, 1.0f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);

#if defined(VOUT_WORLD_POS)
	Out.WorldPos = worldPos.xyz;
#endif
	
	Out.UV0 = In.UV0;
	
#if MAX_UV_CHANNEL_IDX >= 1
	Out.UV1 = In.UV1;
#endif

	Out.Color = InstancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor * In.Color;
	
	Out.TBN = BuildTBN(In.Normal, In.Tangent, InstancedTransformParams[transformIdx].g_transposeInvModel);
	
	Out.InstanceID = In.InstanceID;
	
	return Out;	
}