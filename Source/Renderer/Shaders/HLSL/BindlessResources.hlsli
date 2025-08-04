// © 2025 Adam Badke. All rights reserved.
#ifndef BINDLESS_RESOURCES
#define BINDLESS_RESOURCES

// ---------------------------------------------------------------------------------------------------------------------
// Bindless resources
// Note: We use register spaces to overlap unbounded arrays on index 0
// ---------------------------------------------------------------------------------------------------------------------
#include "../Common/CameraParams.h"
#include "../Common/MaterialParams.h"
#include "../Common/RayTracingParams.h"
#include "../Common/ResourceCommon.h"
#include "../Common/RTAOParams.h"
#include "../Common/TransformParams.h"


// TODO: Use code generation to populate this and automate the space assignments


// CBV Buffers:
ConstantBuffer<CameraData> CameraParams[] : register(b0, space20);
ConstantBuffer<TraceRayData> TraceRayParams[] : register(b0, space21);
ConstantBuffer<DescriptorIndexData> DescriptorIndexes[] : register(b0, space22);
ConstantBuffer<RTAOParamsData> RTAOParams[] : register(b0, space23);

// SRV Buffers:
StructuredBuffer<VertexStreamLUTData> VertexStreamLUTs[] : register(t0, space20);
StructuredBuffer<InstancedBufferLUTData> InstancedBufferLUTs[] : register(t0, space21);
StructuredBuffer<TransformData> TransformParams[] : register(t0, space22);
StructuredBuffer<PBRMetallicRoughnessData> PBRMetallicRoughnessParams[] : register(t0, space23);
StructuredBuffer<UnlitData> UnlitParams[] : register(t0, space24);

// SRV RaytracingAccelerationStructure:
RaytracingAccelerationStructure SceneBVH[] : register(t0, space25); // TLAS

// SRV Textures:
Texture2D<float> Texture2DFloat[] : register(t0, space26);
Texture2D<float4> Texture2DFloat4[] : register(t0, space27);

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
RWTexture2D<float> Texture2DRWFloat[]	: register(u0, space20);
RWTexture2D<float2> Texture2DRWFloat2[] : register(u0, space21);
RWTexture2D<float3> Texture2DRWFloat3[] : register(u0, space22);
RWTexture2D<float4> Texture2DRWFloat4[] : register(u0, space23);


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

struct VertexData
{
	uint m_vertexIdx;
	float3 m_position;
	float3 m_normal; // Geometry normal (i.e. not from normal map)
	float4 m_tangent;
	float2 m_UV0;
	float2 m_UV1;
	float4 m_color;
};
struct TriangleData
{
	VertexData m_v0;
	VertexData m_v1;
	VertexData m_v2;
};
TriangleData LoadTriangleData(uint geoIdx, uint vertexStreamsLUTIdx)
{	
	const uint3 vertexIndexes = GetVertexIndexes(vertexStreamsLUTIdx, geoIdx);
	const StructuredBuffer<VertexStreamLUTData> vertexStreamLUT = VertexStreamLUTs[vertexStreamsLUTIdx];
	
	TriangleData triangleData;
	
	// Vertex indexes:	
	triangleData.m_v0.m_vertexIdx = vertexIndexes.x;
	triangleData.m_v1.m_vertexIdx = vertexIndexes.y;
	triangleData.m_v2.m_vertexIdx = vertexIndexes.z;
	
	// Positions:
	triangleData.m_v0.m_position = VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.x][vertexIndexes.x];
	triangleData.m_v1.m_position = VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.x][vertexIndexes.y];
	triangleData.m_v2.m_position = VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.x][vertexIndexes.z];
	
	// Normals:
	triangleData.m_v0.m_normal = VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.y][vertexIndexes.x];
	triangleData.m_v1.m_normal = VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.y][vertexIndexes.y];
	triangleData.m_v2.m_normal = VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.y][vertexIndexes.z];
	
	// Tangents:
	triangleData.m_v0.m_tangent = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.z][vertexIndexes.x];
	triangleData.m_v1.m_tangent = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.z][vertexIndexes.y];
	triangleData.m_v2.m_tangent = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.z][vertexIndexes.z];
	
	if (vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.w != INVALID_RESOURCE_IDX)
	{
		triangleData.m_v0.m_UV0 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.w][vertexIndexes.x];
		triangleData.m_v1.m_UV0 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.w][vertexIndexes.y];
		triangleData.m_v2.m_UV0 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.w][vertexIndexes.z];
	}
	else
	{
		triangleData.m_v0.m_UV0 = float2(0.f, 0.f);
		triangleData.m_v1.m_UV0 = float2(0.f, 0.f);
		triangleData.m_v2.m_UV0 = float2(0.f, 0.f);
	}
	
	if (vertexStreamLUT[geoIdx].g_UV1ColorIndex.x != INVALID_RESOURCE_IDX)
	{
		triangleData.m_v0.m_UV1 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_UV1ColorIndex.x][vertexIndexes.x];
		triangleData.m_v1.m_UV1 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_UV1ColorIndex.x][vertexIndexes.y];
		triangleData.m_v2.m_UV1 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_UV1ColorIndex.x][vertexIndexes.z];
	}
	else
	{
		triangleData.m_v0.m_UV1 = float2(0.f, 0.f);
		triangleData.m_v1.m_UV1 = float2(0.f, 0.f);
		triangleData.m_v2.m_UV1 = float2(0.f, 0.f);
	}
	
	// Color:
	triangleData.m_v0.m_color = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_UV1ColorIndex.y][vertexIndexes.x];
	triangleData.m_v1.m_color = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_UV1ColorIndex.y][vertexIndexes.y];
	triangleData.m_v2.m_color = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_UV1ColorIndex.y][vertexIndexes.z];
	
	return triangleData;
}


//TriangleData GetWorldTriangleData(TriangleData triangleData, float4x4 localToWorldMatrix)
//{
//	TriangleData worldTriangleData = triangleData;
	
//	worldTriangleData.m_v0.m_position = mul(localToWorldMatrix, float4(triangleData.m_v0.m_position, 1.f)).xyz;
//	worldTriangleData.m_v1.m_position = mul(localToWorldMatrix, float4(triangleData.m_v1.m_position, 1.f)).xyz;
//	worldTriangleData.m_v2.m_position = mul(localToWorldMatrix, float4(triangleData.m_v2.m_position, 1.f)).xyz;


	
//	return worldTriangleData;
//}


struct InterpolatedTriangleData
{
	float2 m_UV0;
	float2 m_UV1;
	float4 m_color;
	
	// TODO: Add remaining hit point properties
};
InterpolatedTriangleData InterpolateTriangleData(TriangleData triangleData, float3 barycentrics)
{
	InterpolatedTriangleData hitPoint;
	
	// UVs:
	hitPoint.m_UV0 =
		triangleData.m_v0.m_UV0.xy * barycentrics.x +
		triangleData.m_v1.m_UV0.xy * barycentrics.y +
		triangleData.m_v2.m_UV0.xy * barycentrics.z;
	hitPoint.m_UV1 =
		triangleData.m_v0.m_UV1.xy * barycentrics.x +
		triangleData.m_v1.m_UV1.xy * barycentrics.y +
		triangleData.m_v2.m_UV1.xy * barycentrics.z;
	
	// Wrap the UVs (accounting for negative values, or values out of [0,1]):
	hitPoint.m_UV0 = hitPoint.m_UV0 - floor(hitPoint.m_UV0);
	hitPoint.m_UV1 = hitPoint.m_UV1 - floor(hitPoint.m_UV1);
	
	// Color:
	hitPoint.m_color =
		triangleData.m_v0.m_color * barycentrics.x +
		triangleData.m_v1.m_color * barycentrics.y +
		triangleData.m_v2.m_color * barycentrics.z;
	
	return hitPoint;
}


struct MaterialData
{
	float4 m_linearAlbedo;
	float4 m_baseColorFactor;
	
	// TODO: Add remaining material properties
};
MaterialData LoadMaterialData(
	InterpolatedTriangleData interpolatedTriData, uint materialResourceIdx, uint materialBufferIdx, uint materialType)
{
	MaterialData materialData;
	
	// Get our indexes:
	uint baseColorResourceIdx = INVALID_RESOURCE_IDX;
	uint baseColorUVChannelIdx = INVALID_RESOURCE_IDX;
	switch (materialType)
	{
	case MAT_ID_GLTF_Unlit:
	{
		const StructuredBuffer<UnlitData> materialBuffer = UnlitParams[materialResourceIdx];
		baseColorResourceIdx = materialBuffer[materialBufferIdx].g_bindlessTextureIndexes0.x;
		baseColorUVChannelIdx = materialBuffer[materialBufferIdx].g_uvChannelIndexes0.x;
		materialData.m_baseColorFactor = materialBuffer[materialBufferIdx].g_baseColorFactor;
	}
	break;
	case MAT_ID_GLTF_PBRMetallicRoughness:
	{
		const StructuredBuffer<PBRMetallicRoughnessData> materialBuffer = PBRMetallicRoughnessParams[materialResourceIdx];
		baseColorResourceIdx = materialBuffer[materialBufferIdx].g_bindlessTextureIndexes0.x;
		baseColorUVChannelIdx = materialBuffer[materialBufferIdx].g_uvChannelIndexes0.x;
		materialData.m_baseColorFactor = materialBuffer[materialBufferIdx].g_baseColorFactor;
	}
	break;
	}
	
	
	if (baseColorResourceIdx != INVALID_RESOURCE_IDX && baseColorUVChannelIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> baseColorTex = Texture2DFloat4[baseColorResourceIdx];

		uint3 texDimensions = uint3(0, 0, 0);
		baseColorTex.GetDimensions(0, texDimensions.x, texDimensions.y, texDimensions.z);
				
		uint3 pixelCoords;
		switch (baseColorUVChannelIdx)
		{
		case 0:
		{
			pixelCoords = uint3(texDimensions.xy * interpolatedTriData.m_UV0, 0);				
		}
		break;
		case 1:
		{
			pixelCoords = uint3(texDimensions.xy * interpolatedTriData.m_UV1, 0);
		}
		break;
		}
				
		materialData.m_linearAlbedo = baseColorTex.Load(pixelCoords);
	}
	else
	{
		materialData.m_linearAlbedo = float4(1.f, 1.f, 1.f, 1.f); // GLTF specs: Default base color is (1,1,1)
	}	
	
	return materialData;
}


#endif // BINDLESS_RESOURCES