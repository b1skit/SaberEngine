// © 2023 Adam Badke. All rights reserved.
#ifndef GBUFFER_COMMON
#define GBUFFER_COMMON

#include "SaberCommon.hlsli"


struct GBuffer
{
	float3 LinearAlbedo;
	float3 WorldNormal;

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
	gbuffer.LinearAlbedo = GBufferAlbedo.Sample(Wrap_Linear_Linear, screenUV).rgb;

	gbuffer.WorldNormal = GBufferWNormal.Sample(Wrap_Linear_Linear, screenUV).xyz;

	const float3 RMAO = GBufferRMAO.Sample(Wrap_Linear_Linear, screenUV).rgb;
	gbuffer.LinearRoughness = RMAO.r;
	gbuffer.LinearMetalness = RMAO.g;
	gbuffer.AO = RMAO.b;

#if defined(GBUFFER_EMISSIVE)
	gbuffer.Emissive = GBufferEmissive.Sample(Wrap_Linear_Linear, screenUV).rgb;
#endif

	gbuffer.MatProp0 = GBufferMatProp0.Sample(Wrap_Linear_Linear, screenUV).rgb;

	gbuffer.NonLinearDepth = GBufferDepth.Sample(Wrap_Linear_Linear, screenUV).r;

	return gbuffer;
}


#endif // GBUFFER_COMMON