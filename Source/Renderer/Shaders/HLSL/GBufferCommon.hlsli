// © 2023 Adam Badke. All rights reserved.
#ifndef GBUFFER_COMMON
#define GBUFFER_COMMON
#include "SaberCommon.hlsli"
#include "NormalMapUtils.hlsli"

struct GBufferOut
{
	float4 Albedo : SV_Target0;
	float4 WorldNormal : SV_Target1;
	float4 RMAOVn : SV_Target2;
	float4 Emissive : SV_Target3;
	float4 MatProp0Vn : SV_Target4;
	uint MaterialID : SV_Target5;
};

Texture2D<float4> GBufferAlbedo;
Texture2D<float4> GBufferWNormal;
Texture2D<float4> GBufferRMAO;
Texture2D<float4> GBufferEmissive;
Texture2D<float4> GBufferMatProp0;
Texture2D<uint> GBufferMaterialID;
Texture2D<float> GBufferDepth;


struct GBuffer
{
	float3 LinearAlbedo;
	float3 WorldNormal;
	float3 WorldVertexNormal;

	float LinearRoughness;
	float LinearMetalness;
	float AO;

#if defined(GBUFFER_EMISSIVE)
	float3 Emissive;
#endif
	float3 MatProp0; // .rgb = F0 (Surface response at 0 degrees)
	float NonLinearDepth;
	
	uint MaterialID;
};


GBuffer UnpackGBuffer(float2 pixelCoords)
{
	GBuffer gbuffer;

	const uint3 loadCoords = uint3(pixelCoords.xy, 0);
	
	// Note: All PBR calculations are performed in linear space
	// However, we use sRGB-format input textures, the sRGB->Linear transformation happens for free when writing the 
	// GBuffer, so no need to do the sRGB -> linear conversion here
	gbuffer.LinearAlbedo = GBufferAlbedo.Load(loadCoords).rgb;

	gbuffer.WorldNormal = GBufferWNormal.Load(loadCoords).xyz;

	const float4 RMAOVn = GBufferRMAO.Load(loadCoords);
	gbuffer.LinearRoughness = RMAOVn.r;
	gbuffer.LinearMetalness = RMAOVn.g;
	gbuffer.AO = RMAOVn.b;

#if defined(GBUFFER_EMISSIVE)
	gbuffer.Emissive = GBufferEmissive.Load(loadCoords).rgb;
#endif

	const float4 matProp0Vn = GBufferMatProp0.Load(loadCoords);
	gbuffer.MatProp0 = matProp0Vn.rgb;
	
	// Unpack the vertex normal:
	const float2 packedVertexNormal = float2(RMAOVn.w, matProp0Vn.w);
	gbuffer.WorldVertexNormal = DecodeOctohedralNormal(packedVertexNormal);

	gbuffer.MaterialID = GBufferMaterialID.Load(loadCoords).r;
	
	gbuffer.NonLinearDepth = GBufferDepth.Load(loadCoords).r;

	return gbuffer;
}


#endif // GBUFFER_COMMON