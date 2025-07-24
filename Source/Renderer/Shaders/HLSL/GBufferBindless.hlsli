// © 2025 Adam Badke. All rights reserved.
#ifndef GBUFFER_BINDLESS
#define GBUFFER_BINDLESS
#include "BindlessResources.hlsli"
#include "GBufferCommon.hlsli"

#include "../Common/ResourceCommon.h"


// Unpacks the GBuffer data from bindless resources: Pass INVALID_RESOURCE_IDX for any resource that is not used
GBuffer UnpackBindlessGBuffer(
	uint2 pixelCoords,
	uint baseColorResourceIdx,
	uint worldNormalResourceIdx,
	uint RMAOVnResourceIdx,
	uint emissiveResourceIdx,
	uint matProp0ResourceIdx,
	uint materialIDResourceIdx,
	uint depthResourceIdx)
{
	const uint3 loadCoords = uint3(pixelCoords.xy, 0);
	
	GBuffer gbuffer;
	
	bool hasPackedVertexNormal = true;
	float2 packedVertexNormal = float2(0.f, 0.f);
	
	if (baseColorResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> baseColorTex = Texture2DFloat4[baseColorResourceIdx];
		
		// Note: All PBR calculations are performed in linear space
		// However, we use sRGB-format input textures, the sRGB->Linear transformation happens for free when writing the 
		// GBuffer, so no need to do the sRGB -> linear conversion here
		gbuffer.LinearAlbedo = baseColorTex.Load(loadCoords).rgb;
	}
	else
	{
		gbuffer.LinearAlbedo = float3(1.0f, 1.0f, 1.0f); // GLTF specs: Default base color is (1,1,1)
	}
	
	if (worldNormalResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> worldNormalTex = Texture2DFloat4[worldNormalResourceIdx];
		gbuffer.WorldNormal = worldNormalTex.Load(loadCoords).xyz;
	}
	else
	{
		gbuffer.WorldNormal = float3(0.0f, 0.0f, 0.0f); // Invalid: No normal data
	}
	
	if (RMAOVnResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> rmaovTex = Texture2DFloat4[RMAOVnResourceIdx];
		const float4 RMAOVn = rmaovTex.Load(loadCoords);
		gbuffer.LinearRoughness = RMAOVn.r;
		gbuffer.LinearMetalness = RMAOVn.g;
		gbuffer.AO = RMAOVn.b;
		
		packedVertexNormal.x = RMAOVn.w;
	}
	else
	{
		gbuffer.LinearRoughness = 1.f; // GLTF specs: If a texture is not given, all components are 1
		gbuffer.LinearMetalness = 1.f;
		gbuffer.AO = 1.f;		
		hasPackedVertexNormal = false;
	}
	
#if defined(GBUFFER_EMISSIVE)
	if (emissiveResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> emissiveTex = Texture2DFloat4[emissiveResourceIdx];
		gbuffer.Emissive = emissiveTex.Load(loadCoords).rgb;
	}
	else
	{
		gbuffer.Emissive = float3(1.f, 1.f, 1.0f); // GLTF specs: If a texture is not given, all components are 1
	}
#endif

	if (matProp0ResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> matProp0Tex = Texture2DFloat4[matProp0ResourceIdx];
		const float4 matProp0Vn = matProp0Tex.Load(loadCoords);
		gbuffer.MatProp0 = matProp0Vn.rgb;
		
		packedVertexNormal = matProp0Vn.w;
	}
	else
	{
		gbuffer.MatProp0 = float3(0.04f, 0.04f, 0.04f);		
		hasPackedVertexNormal = false;
	}

	// Unpck the vertex normal if we can:
	if (hasPackedVertexNormal)
	{
		gbuffer.VertexNormal = DecodeOctohedralNormal(packedVertexNormal);
	}
	else
	{
		gbuffer.VertexNormal = float3(0.f, 0.f, 0.f);
	}
	
	if (materialIDResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<uint> materialIDTex = Texture2DUint[materialIDResourceIdx];
		gbuffer.MaterialID = materialIDTex.Load(loadCoords).r;
	}
	else
	{
		gbuffer.MaterialID = 0; // Default to "unlit" material ID
	}

	if (depthResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float> depthTex = Texture2DFloat[depthResourceIdx];
		gbuffer.NonLinearDepth = depthTex.Load(loadCoords).r;
	}
	else
	{
		gbuffer.NonLinearDepth = 0.f;
	}

	return gbuffer;
}


// Convenience helper: Converts the UVs to pixel coordinates and calls the above function
GBuffer UnpackBindlessGBuffer(
	float2 uv,
	uint baseColorResourceIdx,
	uint worldNormalResourceIdx,
	uint RMAOVnResourceIdx,
	uint emissiveResourceIdx,
	uint matProp0ResourceIdx,
	uint materialIDResourceIdx,
	uint depthResourceIdx)
{
	uint3 gBufferDimensions = uint3(0, 0, 0);
	
	if (baseColorResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> baseColorTex = Texture2DFloat4[baseColorResourceIdx];
		baseColorTex.GetDimensions(0, gBufferDimensions.x, gBufferDimensions.y, gBufferDimensions.z);
	}
	else if (worldNormalResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> worldNormalTex = Texture2DFloat4[worldNormalResourceIdx];
		worldNormalTex.GetDimensions(0, gBufferDimensions.x, gBufferDimensions.y, gBufferDimensions.z);
	}
	else if (RMAOVnResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> rmaovTex = Texture2DFloat4[RMAOVnResourceIdx];
		rmaovTex.GetDimensions(0, gBufferDimensions.x, gBufferDimensions.y, gBufferDimensions.z);
	}
	else	
#if defined(GBUFFER_EMISSIVE)
	if (emissiveResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> emissiveTex = Texture2DFloat4[emissiveResourceIdx];
		emissiveTex.GetDimensions(0, gBufferDimensions.x, gBufferDimensions.y, gBufferDimensions.z);
	}
	else
#endif
	if (matProp0ResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float4> matProp0Tex = Texture2DFloat4[matProp0ResourceIdx];
		matProp0Tex.GetDimensions(0, gBufferDimensions.x, gBufferDimensions.y, gBufferDimensions.z);
	}
	else if (materialIDResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<uint> materialIDTex = Texture2DUint[materialIDResourceIdx];
		materialIDTex.GetDimensions(0, gBufferDimensions.x, gBufferDimensions.y, gBufferDimensions.z);
	}
	else if (depthResourceIdx != INVALID_RESOURCE_IDX)
	{
		Texture2D<float> depthTex = Texture2DFloat[depthResourceIdx];
		depthTex.GetDimensions(0, gBufferDimensions.x, gBufferDimensions.y, gBufferDimensions.z);
	}
	
	// Convert the UVs to pixel coordinates:
	const uint3 pixelCoords = uint3(gBufferDimensions.xy * uv, 0);
	
	return UnpackBindlessGBuffer(
		pixelCoords.xy,
		baseColorResourceIdx,
		worldNormalResourceIdx,
		RMAOVnResourceIdx,
		emissiveResourceIdx,
		matProp0ResourceIdx,
		materialIDResourceIdx,
		depthResourceIdx);
}


#endif