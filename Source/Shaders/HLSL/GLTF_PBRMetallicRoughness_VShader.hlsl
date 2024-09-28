// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_TBN
#define VOUT_COLOR
#define SABER_INSTANCING
#include "NormalMapUtils.hlsli"
#include "SaberCommon.hlsli"

#include "../Generated/HLSL/VertexStreams_PosNmlTanUvCol.hlsli"


#if defined(HAS_MORPH_TARGETS)

StructuredBuffer<float4> MorphData;


float3 UnpackFloat3(
	uint vertexIndex, // For the current vertex
	uint firstFloatIdx, // Within the buffer of interleaved morph target data
	uint vertexStride, // No. of floats for all targets for a single vertex
	uint elementStride, // No. of floats in the element type we're retrieving
	uint morphTargetIdx) // Index of the morph target for the current vertex attribute
{
	float3 result = float3(0.f, 0.f, 0.f);
	
	[unroll]
	for (uint i = 0; i < 3; ++i)
	{
		const uint baseIdx = firstFloatIdx + (vertexIndex * vertexStride) + (morphTargetIdx * elementStride) + i;
		
		const uint idxFloat4 = baseIdx / 4;
		const uint idxComponent = baseIdx % 4;

		result[i] = MorphData[idxFloat4][idxComponent];
	}
	return result;
}

#endif


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const uint transformIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_transformIdx;
	const uint materialIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_materialIdx;
	
	float3 position = In.Position;
	
	// TODO: Implement this correctly. For now, just prove we're getting the data we need
#if defined(HAS_MORPH_TARGETS)
	const uint firstFloatIdx = 0;
	const uint vertexStride = 6;
	const uint elementStride = 3;
	const uint morphTargetIdx = 0;
	
	position += UnpackFloat3(In.VertexID, firstFloatIdx, vertexStride, elementStride, morphTargetIdx);
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