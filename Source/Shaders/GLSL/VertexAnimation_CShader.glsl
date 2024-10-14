#version 460 // Suppress IDE warnings; Stripped out at compile time

#include "../Common/AnimationParams.h"

// OpenGL supports a max of 16 SSBOs in a compute shader - We'll need to change strategies here
#define TEMP_HACK_MAX_SSBOS_ALLOWED 8

layout(std430, binding=0) readonly buffer InVertexStreams
{
	float data[];
} _InVertexStream[TEMP_HACK_MAX_SSBOS_ALLOWED];

layout(std430, binding=TEMP_HACK_MAX_SSBOS_ALLOWED) buffer OutVertexStreams
{
	float data[];
} _OutVertexStreams[TEMP_HACK_MAX_SSBOS_ALLOWED];

layout(binding=16) uniform VertexStreamMetadataParams { VertexStreamMetadata _VertexStreamMetadataParams; };


vec2 UnpackVertexElementAsVec2(uint elementIdx, const uint srcBufferIdx)
{
	const uint baseIdx = elementIdx * 2;	
	return vec2(_InVertexStream[srcBufferIdx].data[baseIdx], _InVertexStream[srcBufferIdx].data[baseIdx + 1]);
}

vec3 UnpackVertexElementAsVec3(uint elementIdx, const uint srcBufferIdx)
{
	const uint baseIdx = elementIdx * 3;
	return vec3(
		_InVertexStream[srcBufferIdx].data[baseIdx],
		_InVertexStream[srcBufferIdx].data[baseIdx + 1],
		_InVertexStream[srcBufferIdx].data[baseIdx + 2]);
}

vec4 UnpackVertexElementAsVec4(uint elementIdx, const uint srcBufferIdx)
{
	const uint baseIdx = elementIdx * 4;
	return vec4(
		_InVertexStream[srcBufferIdx].data[baseIdx],
		_InVertexStream[srcBufferIdx].data[baseIdx + 1],
		_InVertexStream[srcBufferIdx].data[baseIdx + 2],
		_InVertexStream[srcBufferIdx].data[baseIdx + 3]);
}


void PackVertexElementAsVec2(uint elementIdx, vec2 value, const uint dstBufferIdx)
{
	const uint baseIdx = elementIdx * 2;
	_OutVertexStreams[dstBufferIdx].data[baseIdx] = value.x;
	_OutVertexStreams[dstBufferIdx].data[baseIdx + 1] = value.y;
}

void PackVertexElementAsVec3(uint elementIdx, vec3 value, const uint dstBufferIdx)
{
	const uint baseIdx = elementIdx * 3;
	_OutVertexStreams[dstBufferIdx].data[baseIdx] = value.x;
	_OutVertexStreams[dstBufferIdx].data[baseIdx + 1] = value.y;
	_OutVertexStreams[dstBufferIdx].data[baseIdx + 2] = value.z;
}

void PackVertexElementAsVec4(uint elementIdx, vec4 value, const uint dstBufferIdx)
{
	const uint baseIdx = elementIdx * 4;
	_OutVertexStreams[dstBufferIdx].data[baseIdx] = value.x;
	_OutVertexStreams[dstBufferIdx].data[baseIdx + 1] = value.y;
	_OutVertexStreams[dstBufferIdx].data[baseIdx + 2] = value.z;
	_OutVertexStreams[dstBufferIdx].data[baseIdx + 3] = value.w;
}


layout (local_size_x = VERTEX_ANIM_THREADS_X, local_size_y = 1, local_size_z = 1) in;
void CShader()
{
	const uint numVertsPerStream = _VertexStreamMetadataParams.g_meshPrimMetadata.x;
	
	// Early out if our thread index doesn't correspond to a valid vertex index
	const uint vertexIndex = gl_GlobalInvocationID.x;
	if (vertexIndex >= numVertsPerStream)
	{
		return;
	}
	
	// Each compute thread processes the same vertex in every stream:
	for (uint streamIdx = 0; streamIdx < TEMP_HACK_MAX_SSBOS_ALLOWED; ++streamIdx)
	{
		const uint4 streamMetadata = _VertexStreamMetadataParams.g_perStreamMetadata[streamIdx];
		
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
			const float srcData = _InVertexStream[streamIdx].data[vertexIndex];
			_OutVertexStreams[streamIdx].data[vertexIndex] = srcData;
		}
		break;
		case 2:
		{
			const vec2 srcData = UnpackVertexElementAsVec2(vertexIndex, streamIdx);
			PackVertexElementAsVec2(vertexIndex, srcData, streamIdx);
		}
		break;
		case 3:
		{
			const vec3 srcData = UnpackVertexElementAsVec3(vertexIndex, streamIdx);
			PackVertexElementAsVec3(vertexIndex, srcData, streamIdx);
		}
		break;
		case 4:
		{
			const vec4 srcData = UnpackVertexElementAsVec4(vertexIndex, streamIdx);
			PackVertexElementAsVec4(vertexIndex, srcData, streamIdx);
		}
		break;
		default:
		{
			break; // This should never happen
		}
		}
	}
}