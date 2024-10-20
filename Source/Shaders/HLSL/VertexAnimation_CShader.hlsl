// © 2024 Adam Badke. All rights reserved.
#define SPECIFY_OWN_COMPUTE_OUTPUTS
#include "SaberComputeCommon.hlsli"

#include "../Common/AnimationParams.h"


// 1 buffer for each (potential) source vertex stream slot
StructuredBuffer<float> InVertexStreams[MAX_STREAMS_PER_DISPATCH];
RWStructuredBuffer<float> OutVertexStreams[MAX_STREAMS_PER_DISPATCH];

StructuredBuffer<float> MorphData; // Interleaved per-vertex morph displacements
StructuredBuffer<float> MorphWeights;

ConstantBuffer<VertexStreamMetadata> VertexStreamMetadataParams;
ConstantBuffer<DispatchMetadata> DispatchMetadataParams;


float GetVertexComponentValue(uint vertexIdx, uint componentIdx, uint floatStride, StructuredBuffer<float> srcBuffer)
{
	return srcBuffer[NonUniformResourceIndex((vertexIdx * floatStride) + componentIdx)];
}


void SetVertexComponentValue(
	uint vertexIdx, uint componentIdx, uint floatStride, RWStructuredBuffer<float> dstBuffer, float value)
{
	dstBuffer[NonUniformResourceIndex((vertexIdx * floatStride) + componentIdx)] = value;
}


float GetMorphComponentValue(
	uint vertexIdx,
	uint interleavedMorphStride,
	uint componentIdx,
	uint firstFloatOffset,
	uint morphFloatStride,
	uint morphIdx,
	StructuredBuffer<float> morphData)
{
	const uint srcIdx = (vertexIdx * interleavedMorphStride) + firstFloatOffset + (morphIdx * morphFloatStride) + componentIdx;
	return morphData[NonUniformResourceIndex(srcIdx)];
}


[numthreads(VERTEX_ANIM_THREADS_X, 1, 1)]
void CShader(ComputeIn In)
{
	const uint numVertsPerStream = VertexStreamMetadataParams.g_meshPrimMetadata.x;
	
	// Early out if our thread index doesn't correspond to a valid vertex index
	const uint vertexIndex = In.DTId.x;
	if (vertexIndex >= numVertsPerStream)
	{
		return;
	}
	
	const uint numStreamBuffers = DispatchMetadataParams.g_dispatchMetadata.x;
	
	const uint maxMorphTargets = VertexStreamMetadataParams.g_meshPrimMetadata.y;
	const uint interleavedMorphStride = VertexStreamMetadataParams.g_meshPrimMetadata.z; // All displacements combined
	
	// Each compute thread processes the same vertex in every stream:
	for (uint streamIdx = 0; streamIdx < numStreamBuffers; ++streamIdx)
	{		
		const uint vertexFloatStride = VertexStreamMetadataParams.g_streamMetadata[streamIdx].x;
		const uint numVertComponents = VertexStreamMetadataParams.g_streamMetadata[streamIdx].y;
		
		const uint firstMorphFloatOffset = VertexStreamMetadataParams.g_morphMetadata[streamIdx].x;
		const uint displacementFloatStride = VertexStreamMetadataParams.g_morphMetadata[streamIdx].y; // 1 displacement
		const uint numMorphComponents = VertexStreamMetadataParams.g_morphMetadata[streamIdx].z;
		
		// Apply the morph weights iteratively, per component:
		for (uint componentIdx = 0; componentIdx < numMorphComponents; ++componentIdx)
		{
			float vertexComponentVal =
				GetVertexComponentValue(vertexIndex, componentIdx, vertexFloatStride, InVertexStreams[streamIdx]);
			
			for (uint morphIdx = 0; morphIdx < maxMorphTargets; ++morphIdx)
			{		
				const float morphWeight = MorphWeights[morphIdx];
			
				const float morphComponentVal = GetMorphComponentValue(
					vertexIndex, interleavedMorphStride, componentIdx, firstMorphFloatOffset, displacementFloatStride, morphIdx, MorphData);
				
				vertexComponentVal += morphWeight * morphComponentVal;
			}
			
			SetVertexComponentValue(
				vertexIndex, componentIdx, vertexFloatStride, OutVertexStreams[streamIdx], vertexComponentVal);
		}
		
		// If the vertex has more components than the morph data (e.g. tangent = float4, with float3 displacements), we
		// must copy the remaining components:
		if (numVertComponents > numMorphComponents)
		{
			for (uint vertCmptIdx = numMorphComponents; vertCmptIdx < numVertComponents; ++vertCmptIdx)
			{
				const float vertexComponentVal =
					GetVertexComponentValue(vertexIndex, vertCmptIdx, vertexFloatStride, InVertexStreams[streamIdx]);
				
				SetVertexComponentValue(
					vertexIndex, vertCmptIdx, vertexFloatStride, OutVertexStreams[streamIdx], vertexComponentVal);
			}
		}
	}
}