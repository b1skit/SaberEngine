// © 2025 Adam Badke. All rights reserved.
// ---------------------------------------------------------------------------------------------------------------------
// Bindless resources
// Note: We use register spaces to overlap unbounded arrays on index 0
// ---------------------------------------------------------------------------------------------------------------------
#include "../Common/CameraParams.h"
#include "../Common/BindlessResourceParams.h"

#define INVALID_RESOURCE_HANDLE 0xFFFFFFFF

// Bindless resource index LUT: Maps resources to their indexes in unbounded resource arrays
StructuredBuffer<BindlessLUTData> BindlessLUT[]		: register(t0, space20);


// Buffers:
ConstantBuffer<CameraData> CameraParams[] : register(b0, space21);


// Textures:
RWTexture2D<float4> Texture_RW2D[] : register(u0, space22);


// Vertex streams:
StructuredBuffer<uint16_t> VertexStreams_UShort[]	: register(t0, space23); // 16-bit (uint16_t)
StructuredBuffer<uint> VertexStreams_UInt[]			: register(t0, space24); // 32-bit

StructuredBuffer<float2> VertexStreams_Float2[]		: register(t0, space25);
StructuredBuffer<float3> VertexStreams_Float3[]		: register(t0, space26);
StructuredBuffer<float4> VertexStreams_Float4[]		: register(t0, space27);


// ---------------------------------------------------------------------------------------------------------------------
// Helper functions:
// ---------------------------------------------------------------------------------------------------------------------
uint3 GetVertexIndexes(uint lutDescriptorIdx, uint lutIdx, uint vertexID)
{
	uint3 vertexIndexes = uint3(0, 0, 0);
	
	const StructuredBuffer<BindlessLUTData> bindlessLUT = BindlessLUT[lutDescriptorIdx];
	
	if (bindlessLUT[lutIdx].g_UV1ColorIndex.z != INVALID_RESOURCE_HANDLE)
	{
		const uint indexStreamIdx = bindlessLUT[lutIdx].g_UV1ColorIndex.z; // 16 bit indices
		
		const StructuredBuffer<uint16_t> indexStream = VertexStreams_UShort[indexStreamIdx];
		
		vertexIndexes = uint3(
			indexStream[vertexID + 0],
			indexStream[vertexID + 1],
			indexStream[vertexID + 2]
		);
	}
	else
	{
		const uint indexStreamIdx = bindlessLUT[lutIdx].g_UV1ColorIndex.w; // 32 bit indices
		
		const StructuredBuffer<uint> indexStream = VertexStreams_UInt[indexStreamIdx];
		
		vertexIndexes = uint3(
			indexStream[vertexID + 0],
			indexStream[vertexID + 1],
			indexStream[vertexID + 2]
		);
	}
	
	return vertexIndexes;
}