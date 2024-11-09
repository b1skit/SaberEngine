#version 460 // Suppress IDE warnings; Stripped out at compile time

#include "../Common/AnimationParams.h"


layout(std430, binding=0) readonly buffer InVertexStreams
{
	float data[];
} _InVertexStream[MAX_STREAMS_PER_DISPATCH];

layout(std430, binding=MAX_STREAMS_PER_DISPATCH) buffer OutVertexStreams
{
	float data[];
} _OutVertexStreams[MAX_STREAMS_PER_DISPATCH];

layout(std430, binding=16) readonly buffer MorphData { float data[]; } _MorphData; // Interleaved per-vertex morph displacements
layout(std430, binding=17) readonly buffer MorphWeights { float data[]; } _MorphWeights;

layout(binding=18) uniform MorphMetadataParams { MorphMetadata _MorphMetadataParams; };
layout(binding=19) uniform MorphDispatchMetadataParams { MorphDispatchMetadata _MorphDispatchMetadataParams; };


float GetVertexComponentValue(uint vertexIdx, uint componentIdx, uint floatStride, uint srcBufferIdx)
{
	return _InVertexStream[srcBufferIdx].data[(vertexIdx * floatStride) + componentIdx];
}


void SetVertexComponentValue(
	uint vertexIdx, uint componentIdx, uint floatStride, uint dstBufferIdx, float value)
{
	_OutVertexStreams[dstBufferIdx].data[(vertexIdx * floatStride) + componentIdx] = value;
}


float GetMorphComponentValue(
	uint vertexIdx,
	uint interleavedMorphStride,
	uint componentIdx,
	uint firstFloatOffset,
	uint morphFloatStride,
	uint morphIdx)
{
	const uint srcIdx = (vertexIdx * interleavedMorphStride) + firstFloatOffset + (morphIdx * morphFloatStride) + componentIdx;
	return _MorphData.data[srcIdx];
}


layout (local_size_x = VERTEX_ANIM_THREADS_X, local_size_y = 1, local_size_z = 1) in;
void CShader()
{
	const uint numVertsPerStream = _MorphMetadataParams.g_meshPrimMetadata.x;
	
	// Early out if our thread index doesn't correspond to a valid vertex index
	const uint vertexIndex = gl_GlobalInvocationID.x;
	if (vertexIndex >= numVertsPerStream)
	{
		return;
	}
	
	const uint numStreamBuffers = _MorphDispatchMetadataParams.g_dispatchMetadata.x;
	
	const uint maxMorphTargets = _MorphMetadataParams.g_meshPrimMetadata.y;
	const uint interleavedMorphStride = _MorphMetadataParams.g_meshPrimMetadata.z; // All displacements combined

	// Each compute thread processes the same vertex in every stream:
	for (uint streamIdx = 0; streamIdx < numStreamBuffers; ++streamIdx)
	{			
		const uint vertexFloatStride = _MorphMetadataParams.g_streamMetadata[streamIdx].x;
		const uint numVertComponents = _MorphMetadataParams.g_streamMetadata[streamIdx].y;
		
		const uint firstMorphFloatOffset = _MorphMetadataParams.g_morphMetadata[streamIdx].x;
		const uint displacementFloatStride = _MorphMetadataParams.g_morphMetadata[streamIdx].y; // 1 displacement
		const uint numMorphComponents = _MorphMetadataParams.g_morphMetadata[streamIdx].z;
	
		// We loop over all vertex components, as we still need to copy component data when no corresponding morph data
		// exists (e.g. tangent = float4, with float3 displacements)
		for (uint componentIdx = 0; componentIdx < numVertComponents; ++componentIdx)
		{
			float vertexComponentVal =
				GetVertexComponentValue(vertexIndex, componentIdx, vertexFloatStride, streamIdx);
			
			if (componentIdx < numMorphComponents)
			{
				for (uint morphIdx = 0; morphIdx < maxMorphTargets; ++morphIdx)
				{		
					const float morphWeight = _MorphWeights.data[morphIdx];
			
					const float morphDisplacement = GetMorphComponentValue(
						vertexIndex,
						interleavedMorphStride,
						componentIdx,
						firstMorphFloatOffset,
						displacementFloatStride,
						morphIdx);
				
					vertexComponentVal += morphWeight * morphDisplacement;
				}
			}
			
			SetVertexComponentValue(
				vertexIndex, componentIdx, vertexFloatStride, streamIdx, vertexComponentVal);
		}
	}
}