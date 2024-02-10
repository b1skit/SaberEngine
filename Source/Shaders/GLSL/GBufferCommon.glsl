// © 2023 Adam Badke. All rights reserved.
#ifndef GBUFFER_COMMON
#define GBUFFER_COMMON


struct GBuffer
{
	vec3 LinearAlbedo;
	vec3 WorldNormal;

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

	const vec3 RMAO = texture(GBufferRMAO, screenUV).rgb;
	gbuffer.LinearRoughness = RMAO.r;
	gbuffer.LinearMetalness = RMAO.g;
	gbuffer.AO = RMAO.b;

#if defined(GBUFFER_EMISSIVE)
	gbuffer.Emissive = texture(GBufferEmissive, screenUV).rgb;
#endif

	gbuffer.MatProp0 = texture(GBufferMatProp0, screenUV).rgb;

	gbuffer.NonLinearDepth = texture(GBufferDepth, screenUV).r;

	return gbuffer;
}


#endif // GBUFFER_COMMON