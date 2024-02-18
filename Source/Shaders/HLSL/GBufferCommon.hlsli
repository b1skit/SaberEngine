// © 2023 Adam Badke. All rights reserved.
#ifndef GBUFFER_COMMON
#define GBUFFER_COMMON
#include "SaberCommon.hlsli"
#include "NormalMapUtils.hlsli"


struct GBuffer
{
	float3 LinearAlbedo;
	float3 WorldNormal;
	float3 VertexNormal;

	float LinearRoughness;
	float LinearMetalness;
	float AO;

#if defined(GBUFFER_EMISSIVE)
	float3 Emissive;
#endif
	float3 MatProp0; // .rgb = F0 (Surface response at 0 degrees)
	float NonLinearDepth;
};


GBuffer UnpackGBuffer(float2 screenUV)
{
	GBuffer gbuffer;

	// Note: All PBR calculations are performed in linear space
	// However, we use sRGB-format input textures, the sRGB->Linear transformation happens for free when writing the 
	// GBuffer, so no need to do the sRGB -> linear conversion here
	gbuffer.LinearAlbedo = GBufferAlbedo.Sample(WrapMinMagLinearMipPoint, screenUV).rgb;

	gbuffer.WorldNormal = GBufferWNormal.Sample(WrapMinMagLinearMipPoint, screenUV).xyz;

	const float4 RMAOVn = GBufferRMAO.Sample(WrapMinMagLinearMipPoint, screenUV);
	gbuffer.LinearRoughness = RMAOVn.r;
	gbuffer.LinearMetalness = RMAOVn.g;
	gbuffer.AO = RMAOVn.b;

#if defined(GBUFFER_EMISSIVE)
	gbuffer.Emissive = GBufferEmissive.Sample(WrapMinMagLinearMipPoint, screenUV).rgb;
#endif

	const float4 matProp0Vn = GBufferMatProp0.Sample(WrapMinMagLinearMipPoint, screenUV);
	gbuffer.MatProp0 = matProp0Vn.rgb;
	
	// Unpack the vertex normal:
	const float2 packedVertexNormal = float2(RMAOVn.w, matProp0Vn.w);
	gbuffer.VertexNormal = DecodeOctohedralNormal(packedVertexNormal);

	gbuffer.NonLinearDepth = GBufferDepth.Sample(WrapMinMagLinearMipPoint, screenUV).r;

	return gbuffer;
}


#endif // GBUFFER_COMMON