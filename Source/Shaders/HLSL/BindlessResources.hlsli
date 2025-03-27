// © 2025 Adam Badke. All rights reserved.
// ---------------------------------------------------------------------------------------------------------------------
// Bindless resources
// Note: We use register spaces to overlap unbounded arrays on register 0
// ---------------------------------------------------------------------------------------------------------------------
#include "../Common/BindlessResourceParams.h"

#define INVALID_RESOURCE_HANDLE 0xFFFFFFFF

// Bindless resource index LUT: Maps resources to their indexes in unbounded resource arrays
ConstantBuffer<BindlessLUTData> BindlessLUT[]		: register(b0, space10);

// Vertex streams:
StructuredBuffer<uint16_t> VertexStreams_UShort[]	: register(t0, space11); // 16-bit (uint16_t)
StructuredBuffer<uint> VertexStreams_UInt[]			: register(t0, space12); // 32-bit

StructuredBuffer<float2> VertexStreams_Float2[]		: register(t0, space13);
StructuredBuffer<float3> VertexStreams_Float3[]		: register(t0, space14);
StructuredBuffer<float4> VertexStreams_Float4[]		: register(t0, space15);


// ---------------------------------------------------------------------------------------------------------------------
// Helper functions:
// ---------------------------------------------------------------------------------------------------------------------
uint3 GetVertexIndexes(uint lutIdx, uint vertexID)
{
	uint3 vertexIndexes = uint3(0, 0, 0);
	
	if (BindlessLUT[lutIdx].g_UV1ColorIndex.z != INVALID_RESOURCE_HANDLE)
	{
		const uint indexStreamIdx = BindlessLUT[lutIdx].g_UV1ColorIndex.z; // 16 bit indices
		StructuredBuffer<uint16_t> indexStream = VertexStreams_UShort[indexStreamIdx];
		
		vertexIndexes = uint3(
			indexStream[vertexID + 0],
			indexStream[vertexID + 1],
			indexStream[vertexID + 2]
		);
	}
	else
	{
		const uint indexStreamIdx = BindlessLUT[lutIdx].g_UV1ColorIndex.w; // 32 bit indices
		StructuredBuffer<uint> indexStream = VertexStreams_UInt[indexStreamIdx];
		
		vertexIndexes = uint3(
			indexStream[vertexID + 0],
			indexStream[vertexID + 1],
			indexStream[vertexID + 2]
		);
	}
	
	return vertexIndexes;
}