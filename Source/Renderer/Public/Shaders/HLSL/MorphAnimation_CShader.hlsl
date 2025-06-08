// © 2024 Adam Badke. All rights reserved.
#include "SaberComputeCommon.hlsli"

#include "../Common/AnimationParams.h"


// 1 buffer for each (potential) source vertex stream slot
StructuredBuffer<float> InVertexStreams[MAX_STREAMS_PER_DISPATCH];
RWStructuredBuffer<float> OutVertexStreams[MAX_STREAMS_PER_DISPATCH];

StructuredBuffer<float> MorphData; // Interleaved per-vertex morph displacements
StructuredBuffer<float> MorphWeights;

ConstantBuffer<MorphMetadata> MorphMetadataParams;
ConstantBuffer<MorphDispatchMetadata> MorphDispatchMetadataParams;


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
	const uint numVertsPerStream = MorphMetadataParams.g_meshPrimMetadata.x;
	
	// Early out if our thread index doesn't correspond to a valid vertex index
	const uint vertexIndex = In.DTId.x;
	if (vertexIndex >= numVertsPerStream)
	{
		return;
	}
	
	const uint numStreamBuffers = MorphDispatchMetadataParams.g_dispatchMetadata.x;
	
	const uint maxMorphTargets = MorphMetadataParams.g_meshPrimMetadata.y;
	const uint interleavedMorphStride = MorphMetadataParams.g_meshPrimMetadata.z; // All displacements combined
	
	// Each compute thread processes the same vertex in every stream:
	for (uint streamIdx = 0; streamIdx < numStreamBuffers; ++streamIdx)
	{		
		const uint vertexFloatStride = MorphMetadataParams.g_streamMetadata[streamIdx].x;
		const uint numVertComponents = MorphMetadataParams.g_streamMetadata[streamIdx].y;
		
		const uint firstMorphFloatOffset = MorphMetadataParams.g_morphMetadata[streamIdx].x;
		const uint displacementFloatStride = MorphMetadataParams.g_morphMetadata[streamIdx].y; // 1 displacement
		const uint numMorphComponents = MorphMetadataParams.g_morphMetadata[streamIdx].z;
		
		// We loop over all vertex components, as we still need to copy component data when no corresponding morph data
		// exists (e.g. tangent = float4, with float3 displacements)
		for (uint componentIdx = 0; componentIdx < numVertComponents; ++componentIdx)
		{
			float vertexComponentVal =
				GetVertexComponentValue(vertexIndex, componentIdx, vertexFloatStride, InVertexStreams[streamIdx]);
			
			if (componentIdx < numMorphComponents)
			{
				for (uint morphIdx = 0; morphIdx < maxMorphTargets; ++morphIdx)
				{
					const float morphWeight = MorphWeights[morphIdx];
			
					const float morphDisplacement = GetMorphComponentValue(
						vertexIndex,
						interleavedMorphStride,
						componentIdx, 
						firstMorphFloatOffset, 
						displacementFloatStride, 
						morphIdx, 
						MorphData);
				
					vertexComponentVal += morphWeight * morphDisplacement;
				}
			}

			SetVertexComponentValue(
				vertexIndex, componentIdx, vertexFloatStride, OutVertexStreams[streamIdx], vertexComponentVal);
		}
	}
}