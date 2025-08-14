// © 2025 Adam Badke. All rights reserved.
#ifndef BINDLESS_RESOURCES
#define BINDLESS_RESOURCES
#include "NormalMapUtils.hlsli"
#include "RayTracingCommon.hlsli"
#include "Samplers.hlsli"
#include "TextureLODHelpers.hlsli"

#include "../Common/CameraParams.h"
#include "../Common/MaterialParams.h"
#include "../Common/RayTracingParams.h"
#include "../Common/ResourceCommon.h"
#include "../Common/RTAOParams.h"
#include "../Common/TransformParams.h"


// ---------------------------------------------------------------------------------------------------------------------
// Bindless resources
// Note: We use register spaces to overlap unbounded arrays on index 0
// ---------------------------------------------------------------------------------------------------------------------


// TODO: Use code generation to populate this and automate the space assignments


// CBV Buffers:
ConstantBuffer<CameraData> CameraParams[] : register(b0, space20);
ConstantBuffer<TraceRayData> TraceRayParams[] : register(b0, space21);
ConstantBuffer<DescriptorIndexData> DescriptorIndexes[] : register(b0, space22);
ConstantBuffer<RTAOParamsData> RTAOParams[] : register(b0, space23);
ConstantBuffer<TemporalAccumulationData> TemporalAccumulationParams[] : register(b0, space24);

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


TriangleData LoadTriangleData(uint geoIdx, uint vertexStreamsLUTIdx, uint transformResourceIdx,	uint transformBufferIdx)
{
	const TransformData transform = TransformParams[transformResourceIdx][transformBufferIdx];
	
	const uint3 vertexIndexes = GetVertexIndexes(vertexStreamsLUTIdx, geoIdx);
	const StructuredBuffer<VertexStreamLUTData> vertexStreamLUT = VertexStreamLUTs[vertexStreamsLUTIdx];
	
	const float3x3 transposeInvRotationScale = (float3x3)transform.g_transposeInvModel;
	
	TriangleData triangleData;
	
	// Positions:
	triangleData.m_v0.m_worldVertexPosition =
		mul(transform.g_model, float4(VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.x][vertexIndexes.x], 1.f)).xyz;
	triangleData.m_v1.m_worldVertexPosition =
		mul(transform.g_model, float4(VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.x][vertexIndexes.y], 1.f)).xyz;
	triangleData.m_v2.m_worldVertexPosition =
		mul(transform.g_model, float4(VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.x][vertexIndexes.z], 1.f)).xyz;
	
	// Unnormalized vertex normals:
	triangleData.m_v0.m_worldVertexNormal =
		mul(transposeInvRotationScale, VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.y][vertexIndexes.x]).xyz;
	triangleData.m_v1.m_worldVertexNormal =
		mul(transposeInvRotationScale, VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.y][vertexIndexes.y]).xyz;
	triangleData.m_v2.m_worldVertexNormal =
		mul(transposeInvRotationScale, VertexStreams_Float3[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.y][vertexIndexes.z]).xyz;
	
	// Triangle plane normal:
	triangleData.m_worldTriPlaneNormal = normalize(cross(
		triangleData.m_v1.m_worldVertexPosition - triangleData.m_v0.m_worldVertexPosition,
		triangleData.m_v2.m_worldVertexPosition - triangleData.m_v0.m_worldVertexPosition));
	
	// Tangents:
	const float4 v0Tangent = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.z][vertexIndexes.x];
	const float4 v1Tangent = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.z][vertexIndexes.y];
	const float4 v2Tangent = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.z][vertexIndexes.z];
	triangleData.m_v0.m_worldVertexTangent = float4(mul(transposeInvRotationScale, v0Tangent.xyz), v0Tangent.w);
	triangleData.m_v1.m_worldVertexTangent = float4(mul(transposeInvRotationScale, v1Tangent.xyz), v1Tangent.w);
	triangleData.m_v2.m_worldVertexTangent = float4(mul(transposeInvRotationScale, v2Tangent.xyz), v2Tangent.w);
	
	if (vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.w != INVALID_RESOURCE_IDX)
	{
		triangleData.m_v0.m_vertexUV0 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.w][vertexIndexes.x];
		triangleData.m_v1.m_vertexUV0 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.w][vertexIndexes.y];
		triangleData.m_v2.m_vertexUV0 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_posNmlTanUV0Index.w][vertexIndexes.z];
	}
	else
	{
		triangleData.m_v0.m_vertexUV0 = float2(0.f, 0.f);
		triangleData.m_v1.m_vertexUV0 = float2(0.f, 0.f);
		triangleData.m_v2.m_vertexUV0 = float2(0.f, 0.f);
	}
	
	if (vertexStreamLUT[geoIdx].g_UV1ColorIndex.x != INVALID_RESOURCE_IDX)
	{
		triangleData.m_v0.m_vertexUV1 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_UV1ColorIndex.x][vertexIndexes.x];
		triangleData.m_v1.m_vertexUV1 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_UV1ColorIndex.x][vertexIndexes.y];
		triangleData.m_v2.m_vertexUV1 = VertexStreams_Float2[vertexStreamLUT[geoIdx].g_UV1ColorIndex.x][vertexIndexes.z];
	}
	else
	{
		triangleData.m_v0.m_vertexUV1 = float2(0.f, 0.f);
		triangleData.m_v1.m_vertexUV1 = float2(0.f, 0.f);
		triangleData.m_v2.m_vertexUV1 = float2(0.f, 0.f);
	}
	
	// Color:
	triangleData.m_v0.m_vertexColor = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_UV1ColorIndex.y][vertexIndexes.x];
	triangleData.m_v1.m_vertexColor = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_UV1ColorIndex.y][vertexIndexes.y];
	triangleData.m_v2.m_vertexColor = VertexStreams_Float4[vertexStreamLUT[geoIdx].g_UV1ColorIndex.y][vertexIndexes.z];
	
	return triangleData;
}


TriangleHitData GetTriangleHitData(TriangleData triangleData, float3 barycentrics)
{
	TriangleHitData hitData;
	
	// Position:	
	hitData.m_worldHitPosition =
		triangleData.m_v0.m_worldVertexPosition * barycentrics.x +
		triangleData.m_v1.m_worldVertexPosition * barycentrics.y +
		triangleData.m_v2.m_worldVertexPosition * barycentrics.z;
	
	// Tangent:
	hitData.m_worldHitTangent = normalize(
		triangleData.m_v0.m_worldVertexTangent * barycentrics.x +
		triangleData.m_v1.m_worldVertexTangent * barycentrics.y +
		triangleData.m_v2.m_worldVertexTangent * barycentrics.z);
	
	// Interpolated world vertex normal:	
	hitData.m_worldHitNormal = normalize(
		triangleData.m_v0.m_worldVertexNormal * barycentrics.x +
		triangleData.m_v1.m_worldVertexNormal * barycentrics.y +
		triangleData.m_v2.m_worldVertexNormal * barycentrics.z);
	
	// UVs:
	hitData.m_hitUV0 =
		triangleData.m_v0.m_vertexUV0.xy * barycentrics.x +
		triangleData.m_v1.m_vertexUV0.xy * barycentrics.y +
		triangleData.m_v2.m_vertexUV0.xy * barycentrics.z;
	hitData.m_hitUV1 =
		triangleData.m_v0.m_vertexUV1.xy * barycentrics.x +
		triangleData.m_v1.m_vertexUV1.xy * barycentrics.y +
		triangleData.m_v2.m_vertexUV1.xy * barycentrics.z;
	
	// Color:
	hitData.m_hitColor =
		triangleData.m_v0.m_vertexColor * barycentrics.x +
		triangleData.m_v1.m_vertexColor * barycentrics.y +
		triangleData.m_v2.m_vertexColor * barycentrics.z;
	
	return hitData;
}


float4 SampleTexture2DFloat4(
	TriangleData triangleData,
	TriangleHitData hitData,
	BarycentricDerivatives barycentricDerivatives,
	uint srcTexResourceIdx,
	uint uvChannelIdx,
	SamplerState samplerState = WrapMinMagMipLinear)
{
	Texture2D<float4> srcTex = Texture2DFloat4[srcTexResourceIdx];

	uint3 texDims = uint3(0, 0, 0);
	srcTex.GetDimensions(0.f, texDims.x, texDims.y, texDims.z);
		
	const float mipLevel =
		ComputeIsotropicTextureLOD(texDims.xy, barycentricDerivatives, triangleData, uvChannelIdx);
	
	float4 result;
	switch (uvChannelIdx)
	{
	case 1:
	{
		result = srcTex.SampleLevel(samplerState, hitData.m_hitUV1, mipLevel);
	}
	break;
	case 0:
	default:
	{
		result = srcTex.SampleLevel(samplerState, hitData.m_hitUV0, mipLevel);
	}
	break;
	}
	
	return result;
}


struct MaterialData
{
	float4 LinearAlbedo; // srcTex * baseColorFactor
	
	float3 WorldNormal; // Texture normal in world space, normalized

	float LinearMetalness;
	float LinearRoughness;
	float AO;

	float3 Emissive;
	float3 MatProp0; // .rgb = F0 (Surface response at 0 degrees)
	
	float AlphaCutoff;
};
MaterialData LoadMaterialData(
	TriangleData triangleData,
	TriangleHitData hitData,
	RayDifferential transferredRayDiff,	// Transferred ray differentials at the hit point
	uint materialResourceIdx,
	uint materialBufferIdx,
	uint materialType)
{
	const BarycentricDerivatives barycentricDerivatives =
		ComputeBarycentricDerivatives(transferredRayDiff, WorldRayDirection(), triangleData);
	
	MaterialData materialData;
	
	// Default values:
	materialData.LinearAlbedo = float4(1.f, 1.f, 1.f, 1.f); // GLTF specs: Default base color is (1,1,1)
	materialData.WorldNormal = float3(0.f, 0.f, 1.f);
	materialData.LinearRoughness = 1.f;						// GLTF specs: Default roughness is 1 (i.e. fully rough)
	materialData.LinearMetalness = 0.f;						// GLTF specs: Default metalness is 0 (i.e. non-metallic)
	materialData.AO = 1.f;									// GLTF specs: Default occlusion is 1 (i.e. no occlusion)
	materialData.Emissive = float3(0.f, 0.f, 0.f);			// GLTF specs: Default emissive is (0,0,0)
	materialData.MatProp0 = float3(0.04, 0.04f, 0.04f);		// GLTF specs: Default F0 is (0,0,0)
	
	float4 baseColorFactor = float4(1.f, 1.f, 1.f, 1.f);	
	float normalScaleFactor = 1.f;
	float metallicFactor = 1.f;
	float roughnessFactor = 1.f;
	float occlusionStrength = 1.f;
	float3 emissiveFactor = float3(1.f, 1.f, 1.f);
	float emissiveStrength = 1.f;
	
	// Get our indexes:
	uint baseColorResourceIdx = INVALID_RESOURCE_IDX;
	uint baseColorUVChannelIdx = INVALID_RESOURCE_IDX;
	
	uint normalResourceIdx = INVALID_RESOURCE_IDX;
	uint normalUVChannelIdx = INVALID_RESOURCE_IDX;
	
	uint metallicRoughnessResourceIdx = INVALID_RESOURCE_IDX;
	uint metallicRoughnessUVChannelIdx = INVALID_RESOURCE_IDX;
	
	uint occlusionResourceIdx = INVALID_RESOURCE_IDX;
	uint occlusionUVChannelIdx = INVALID_RESOURCE_IDX;
	
	uint emissiveResourceIdx = INVALID_RESOURCE_IDX;
	uint emissiveUVChannelIdx = INVALID_RESOURCE_IDX;
	
	switch (materialType)
	{
	case MAT_ID_GLTF_Unlit:
	{
		const UnlitData materialBuffer = UnlitParams[materialResourceIdx][materialBufferIdx];
			
		baseColorFactor =			materialBuffer.g_baseColorFactor;
		
		baseColorUVChannelIdx =		materialBuffer.g_uvChannelIndexes0.x;
			
		baseColorResourceIdx =		materialBuffer.g_bindlessTextureIndexes0.x;		
			
		materialData.AlphaCutoff =	materialBuffer.g_alphaCutuff.x;
	}
	break;
	case MAT_ID_GLTF_PBRMetallicRoughness:
	{
		const PBRMetallicRoughnessData materialBuffer = PBRMetallicRoughnessParams[materialResourceIdx][materialBufferIdx];
			
		baseColorFactor =				materialBuffer.g_baseColorFactor;
		metallicFactor =				materialBuffer.g_metRoughNmlOccScales.x;		
		roughnessFactor =				materialBuffer.g_metRoughNmlOccScales.y;
		normalScaleFactor =				materialBuffer.g_metRoughNmlOccScales.z;
		occlusionStrength =				materialBuffer.g_metRoughNmlOccScales.w;
		emissiveFactor =				materialBuffer.g_emissiveFactorStrength.rgb;
		emissiveStrength =				materialBuffer.g_emissiveFactorStrength.w;
			
		baseColorUVChannelIdx =			materialBuffer.g_uvChannelIndexes0.x;
		metallicRoughnessUVChannelIdx = materialBuffer.g_uvChannelIndexes0.y;
		normalUVChannelIdx =			materialBuffer.g_uvChannelIndexes0.z;
		occlusionUVChannelIdx =			materialBuffer.g_uvChannelIndexes0.w;
			
		emissiveUVChannelIdx =			materialBuffer.g_uvChannelIndexes1.x;
		
		baseColorResourceIdx =			materialBuffer.g_bindlessTextureIndexes0.x;
		metallicRoughnessResourceIdx =	materialBuffer.g_bindlessTextureIndexes0.y;
		normalResourceIdx =				materialBuffer.g_bindlessTextureIndexes0.z;
		occlusionResourceIdx =			materialBuffer.g_bindlessTextureIndexes0.w;
		
		emissiveResourceIdx =			materialBuffer.g_bindlessTextureIndexes1.x;
			
		materialData.AlphaCutoff =		materialBuffer.g_f0AlphaCutoff.w;
	}
	break;
	}
	
	// Base color:
	if (baseColorResourceIdx != INVALID_RESOURCE_IDX && baseColorUVChannelIdx != INVALID_RESOURCE_IDX)
	{
		materialData.LinearAlbedo = baseColorFactor * hitData.m_hitColor *
			SampleTexture2DFloat4(
				triangleData, 
				hitData, 
				barycentricDerivatives, 
				baseColorResourceIdx, 
				baseColorUVChannelIdx, 
				WrapMinMagMipLinear);
	}
	
	// Normal:
	if (normalResourceIdx != INVALID_RESOURCE_IDX && normalUVChannelIdx != INVALID_RESOURCE_IDX)
	{
		const float4 texNormal = SampleTexture2DFloat4(
			triangleData,
			hitData,
			barycentricDerivatives,
			normalResourceIdx,
			normalUVChannelIdx,
			WrapMinMagMipLinear);
		
		const float3 worldBitangent = 
			normalize(cross(hitData.m_worldHitNormal, hitData.m_worldHitTangent.xyz) * hitData.m_worldHitTangent.w);
		
		// Matrix ctors pack in row-major ordering: Insert our TBN vectors as columns
		const float3x3 TBN = float3x3(
			hitData.m_worldHitTangent.x, worldBitangent.x, hitData.m_worldHitNormal.x,
			hitData.m_worldHitTangent.y, worldBitangent.y, hitData.m_worldHitNormal.y,
			hitData.m_worldHitTangent.z, worldBitangent.z, hitData.m_worldHitNormal.z);
		
		materialData.WorldNormal = WorldNormalFromTextureNormal(texNormal.xyz, normalScaleFactor, TBN);
	}
	
	// MetallicRoughness:
	if (metallicRoughnessResourceIdx != INVALID_RESOURCE_IDX && metallicRoughnessUVChannelIdx != INVALID_RESOURCE_IDX)
	{
		// Unpack/scale metallic/roughness: .G = roughness, .B = metalness
		const float2 roughnessMetalness = SampleTexture2DFloat4(
				triangleData,
				hitData,
				barycentricDerivatives,
				metallicRoughnessResourceIdx,
				metallicRoughnessUVChannelIdx,
				WrapMinMagMipLinear).gb;
		
		materialData.LinearRoughness = roughnessMetalness.x * roughnessFactor;
		materialData.LinearMetalness = roughnessMetalness.y * metallicFactor;
	}
	
	// Occlusion:
	if (occlusionResourceIdx != INVALID_RESOURCE_IDX && occlusionUVChannelIdx != INVALID_RESOURCE_IDX)
	{
		materialData.AO = occlusionStrength * 
			SampleTexture2DFloat4(
				triangleData,
				hitData,
				barycentricDerivatives,
				occlusionResourceIdx,
				occlusionUVChannelIdx,
				WrapMinMagMipLinear).r;
	}
	
	// Emissive:
	if (emissiveResourceIdx != INVALID_RESOURCE_IDX && emissiveUVChannelIdx != INVALID_RESOURCE_IDX)
	{
		materialData.Emissive = emissiveFactor * emissiveStrength *
			SampleTexture2DFloat4(
				triangleData,
				hitData,
				barycentricDerivatives,
				emissiveResourceIdx,
				emissiveUVChannelIdx,
				WrapMinMagMipLinear).rgb;
	}
	
	return materialData;
}


#endif // BINDLESS_RESOURCES