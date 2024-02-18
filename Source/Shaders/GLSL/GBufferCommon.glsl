// © 2023 Adam Badke. All rights reserved.
#ifndef GBUFFER_COMMON
#define GBUFFER_COMMON
#include "NormalMapUtils.glsl"


struct GBuffer
{
	vec3 LinearAlbedo;
	vec3 WorldNormal;
	vec3 VertexNormal;

	float LinearRoughness;
	float LinearMetalness;
	float AO;

#if defined(GBUFFER_EMISSIVE)
	vec3 Emissive;
#endif
	vec3 MatProp0; // .rgb = F0 (Surface response at 0 degrees)
	float NonLinearDepth;
};


GBuffer UnpackGBuffer(vec2 screenUV)
{
	GBuffer gbuffer;

	// Note: All PBR calculations are performed in linear space
	// However, we use sRGB-format input textures, the sRGB->Linear transformation happens for free when writing the 
	// GBuffer, so no need to do the sRGB -> linear conversion here
	gbuffer.LinearAlbedo = texture(GBufferAlbedo, screenUV).rgb;

	gbuffer.WorldNormal = texture(GBufferWNormal, screenUV).xyz;

	const vec4 RMAOVn = texture(GBufferRMAO, screenUV);
	gbuffer.LinearRoughness = RMAOVn.r;
	gbuffer.LinearMetalness = RMAOVn.g;
	gbuffer.AO = RMAOVn.b;

#if defined(GBUFFER_EMISSIVE)
	gbuffer.Emissive = texture(GBufferEmissive, screenUV).rgb;
#endif

	const vec4 matProp0Vn = texture(GBufferMatProp0, screenUV);
	gbuffer.MatProp0 = matProp0Vn.rgb;

	// Unpack the vertex normal:
	const vec2 packedVertexNormal = vec2(RMAOVn.w, matProp0Vn.w);
	gbuffer.VertexNormal = DecodeOctohedralNormal(packedVertexNormal);

	gbuffer.NonLinearDepth = texture(GBufferDepth, screenUV).r;

	return gbuffer;
}


#endif // GBUFFER_COMMON