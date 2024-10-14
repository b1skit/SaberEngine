// © 2024 Adam Badke. All rights reserved.
#define SPECIFY_OWN_COMPUTE_OUTPUTS
#include "SaberComputeCommon.hlsli"

#include "../Common/AnimationParams.h"


// 1 buffer for each (potential) source vertex stream slot
StructuredBuffer<float> InVertexStreams[NUM_VERTEX_STREAMS];
RWStructuredBuffer<float> OutVertexStreams[NUM_VERTEX_STREAMS];

ConstantBuffer<VertexStreamMetadata> VertexStreamMetadataParams;


float2 UnpackVertexElementAsFloat2(uint elementIdx, StructuredBuffer<float> srcBuffer)
{
	const uint baseIdx = elementIdx * 2;	
	return float2(srcBuffer[baseIdx], srcBuffer[baseIdx + 1]);
}

float3 UnpackVertexElementAsFloat3(uint elementIdx, StructuredBuffer<float> srcBuffer)
{
	const uint baseIdx = elementIdx * 3;
	return float3(srcBuffer[baseIdx], srcBuffer[baseIdx + 1], srcBuffer[baseIdx + 2]);
}

float4 UnpackVertexElementAsFloat4(uint elementIdx, StructuredBuffer<float> srcBuffer)
{
	const uint baseIdx = elementIdx * 4;
	return float4(srcBuffer[baseIdx], srcBuffer[baseIdx + 1], srcBuffer[baseIdx + 2], srcBuffer[baseIdx + 3]);
}


void PackVertexElementAsFloat2(uint elementIdx, float2 value, RWStructuredBuffer<float> dstBuffer)
{
	const uint baseIdx = elementIdx * 2;
	dstBuffer[baseIdx] = value.x;
	dstBuffer[baseIdx + 1] = value.y;
}

void PackVertexElementAsFloat3(uint elementIdx, float3 value, RWStructuredBuffer<float> dstBuffer)
{
	const uint baseIdx = elementIdx * 3;
	dstBuffer[baseIdx] = value.x;
	dstBuffer[baseIdx + 1] = value.y;
	dstBuffer[baseIdx + 2] = value.z;
}

void PackVertexElementAsFloat4(uint elementIdx, float4 value, RWStructuredBuffer<float> dstBuffer)
{
	const uint baseIdx = elementIdx * 4;
	dstBuffer[baseIdx] = value.x;
	dstBuffer[baseIdx + 1] = value.y;
	dstBuffer[baseIdx + 2] = value.z;
	dstBuffer[baseIdx + 3] = value.w;
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
	
	// Each compute thread processes the same vertex in every stream:
	for (uint streamIdx = 0; streamIdx < NUM_VERTEX_STREAMS; ++streamIdx)
	{
		const uint4 streamMetadata = VertexStreamMetadataParams.g_perStreamMetadata[streamIdx];
		
		const uint floatStride = streamMetadata.x;
		if (floatStride == 0)
		{
			break; // No more streams to process
		}
		
		// TODO: Implement animation. For now, just copy the data through as a proof of concept
		switch (floatStride)
		{
		case 1:
		{
			const float srcData = InVertexStreams[streamIdx][vertexIndex];
			OutVertexStreams[streamIdx][vertexIndex] = srcData;
		}
		break;
		case 2:
		{
			const float2 srcData = UnpackVertexElementAsFloat2(vertexIndex, InVertexStreams[streamIdx]);
			PackVertexElementAsFloat2(vertexIndex, srcData, OutVertexStreams[streamIdx]);
		}
		break;
		case 3:
		{
			const float3 srcData = UnpackVertexElementAsFloat3(vertexIndex, InVertexStreams[streamIdx]);
			PackVertexElementAsFloat3(vertexIndex, srcData, OutVertexStreams[streamIdx]);
		}
		break;
		case 4:
		{
			const float4 srcData = UnpackVertexElementAsFloat4(vertexIndex, InVertexStreams[streamIdx]);
			PackVertexElementAsFloat4(vertexIndex, srcData, OutVertexStreams[streamIdx]);
		}
		break;
		default:
		{
			break; // This should never happen
		}
		}
	}
}