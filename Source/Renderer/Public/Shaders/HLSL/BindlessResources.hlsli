// © 2025 Adam Badke. All rights reserved.
// ---------------------------------------------------------------------------------------------------------------------
// Bindless resources
// Note: We use register spaces to overlap unbounded arrays on index 0
// ---------------------------------------------------------------------------------------------------------------------
#include "../Common/CameraParams.h"
#include "../Common/MaterialParams.h"
#include "../Common/RayTracingParams.h"
#include "../Common/ResourceCommon.h"
#include "../Common/TransformParams.h"


// TODO: Use code generation to populate this and automate the space assignments

// CBV Buffers:
ConstantBuffer<CameraData> CameraParams[] : register(b0, space20);
ConstantBuffer<TraceRayData> TraceRayParams[] : register(b0, space21);
ConstantBuffer<DescriptorIndexData> DescriptorIndexes[] : register(b0, space22);


// SRV Buffers:
StructuredBuffer<VertexStreamLUTData> VertexStreamLUTs[] : register(t0, space20);
StructuredBuffer<InstancedBufferLUTData> InstancedBufferLUTs[] : register(t0, space21);
StructuredBuffer<TransformData> TransformParams[] : register(t0, space22);
StructuredBuffer<PBRMetallicRoughnessData> PBRMetallicRoughnessParams[] : register(t0, space23);
StructuredBuffer<UnlitData> UnlitParams[] : register(t0, space24);

// SRV RaytracingAccelerationStructure:
RaytracingAccelerationStructure SceneBVH[] : register(t0, space25); // TLAS

// SRV Textures:
Texture2D<float4> Texture2DFloat4[] : register(t0, space26);
Texture2D<float> Texture2DFloat[] : register(t0, space27);
Texture2D<uint> Texture2DUint[] : register(t0, space28);

Texture2DArray<float> Texture2DArrayFloat[] : register(t0, space29);

TextureCube<float4> TextureCubeFloat4[] : register(t0, space30);

TextureCubeArray<float> TextureCubeArrayFloat[] : register(t0, space31);

// SRV Vertex streams:
StructuredBuffer<uint16_t> VertexStreams_UShort[]	: register(t0, space32); // 16-bit (uint16_t)
StructuredBuffer<uint> VertexStreams_UInt[]			: register(t0, space33); // 32-bit

StructuredBuffer<float2> VertexStreams_Float2[]		: register(t0, space34);
StructuredBuffer<float3> VertexStreams_Float3[]		: register(t0, space35);
StructuredBuffer<float4> VertexStreams_Float4[]		: register(t0, space36);


// UAV Textures:
RWTexture2D<float4> Texture2DRWFloat4[] : register(u0, space20);


// ---------------------------------------------------------------------------------------------------------------------
// Helper functions:
// ---------------------------------------------------------------------------------------------------------------------
uint3 GetVertexIndexes(uint vertexStreamsLUTIdx, uint lutIdx)
{
	const uint vertexID = 3 * PrimitiveIndex(); // Triangle index -> Vertex index
	
	uint3 vertexIndexes = uint3(0, 0, 0);
	
	const StructuredBuffer<VertexStreamLUTData> vertexStreamLUT = VertexStreamLUTs[vertexStreamsLUTIdx];
	
	if (vertexStreamLUT[lutIdx].g_UV1ColorIndex.z != INVALID_RESOURCE_IDX)
	{
		const uint indexStreamIdx = vertexStreamLUT[lutIdx].g_UV1ColorIndex.z; // 16 bit indices
		
		const StructuredBuffer<uint16_t> indexStream = VertexStreams_UShort[indexStreamIdx];
		
		vertexIndexes = uint3(
			indexStream[vertexID + 0],
			indexStream[vertexID + 1],
			indexStream[vertexID + 2]
		);
	}
	else
	{
		const uint indexStreamIdx = vertexStreamLUT[lutIdx].g_UV1ColorIndex.w; // 32 bit indices
		
		const StructuredBuffer<uint> indexStream = VertexStreams_UInt[indexStreamIdx];
		
		vertexIndexes = uint3(
			indexStream[vertexID + 0],
			indexStream[vertexID + 1],
			indexStream[vertexID + 2]
		);
	}
	
	return vertexIndexes;
}