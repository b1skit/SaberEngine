// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


layout (location = 0) out vec4 gBuffer_out_albedo;
layout (location = 1) out vec4 gBuffer_out_worldNormal;
layout (location = 2) out vec4 gBuffer_out_RMAO;
layout (location = 3) out vec4 gBuffer_out_emissive;
layout (location = 4) out vec4 gBuffer_out_matProp0;
layout (location = 5) out vec4 gBuffer_out_depth;


void main()
{
	// Albedo. Note: We use an sRGB-format texture, which converts this value from sRGB->linear space for free
	// g_baseColorFactor and vOut.Color are factored into the albedo as per the GLTF 2.0 specifications
	gBuffer_out_albedo = texture(MatAlbedo, vOut.uv0.xy) * g_baseColorFactor * vOut.Color;

	// Alpha clipping:
	if (gBuffer_out_albedo.a < ALPHA_CUTOFF)
	{
		discard;
	}

	// World-space normal:
	const vec3 normalScale = vec3(g_normalScale, g_normalScale, 1.f); // Scales the normal's X, Y directions
	const vec3 texNormal = texture(MatNormal, vOut.uv0.xy).xyz;
	const vec3 worldNormal = WorldNormalFromTextureNormal(texNormal, vOut.TBN) * normalScale;

	gBuffer_out_worldNormal = vec4(worldNormal, 0.0f);
	
	// Unpack/scale metallic/roughness: .G = roughness, .B = metallness
	const vec2 roughMetal = texture(MatMetallicRoughness, vOut.uv0.xy).gb * vec2(g_roughnessFactor, g_metallicFactor);

	// Unpack/scale AO:
	const float occlusion = texture(MatOcclusion, vOut.uv0.xy).r * g_occlusionStrength;
	//const float occlusion = clamp((1.0f + g_occlusionStrength) * (texture(MatOcclusion, vOut.uv0.xy).r - 1.0f), 0.0f, 1.0f);
	// TODO: GLTF specifies the above occlusion scaling, but CGLTF seems non-complicant & packs occlusion strength into
	// the texture scale value. For now, just use something sane.
	
	// Pack RMAO: 
	gBuffer_out_RMAO = vec4(roughMetal, occlusion, 1.0f);

	// Emissive:
	const vec3 emissiveFactor = g_emissiveFactorStrength.rgb;
	const float emissiveStrength = g_emissiveFactorStrength.w;
	vec3 emissive = texture(MatEmissive, vOut.uv0.xy).rgb * emissiveFactor * emissiveStrength;

	// Emissive is light: Apply exposure now:
	const float ev100 = g_exposureProperties.y;

	const float emissiveExposureCompensation = g_exposureProperties.z;
	emissive *= pow(2.0, ev100 + emissiveExposureCompensation - 3.0f);

	const float exposure = g_exposureProperties.x;
	emissive *= exposure;

	gBuffer_out_emissive = vec4(emissive, 1.0f);

	// Material properties:
	gBuffer_out_matProp0 = g_f0; // .xyz = f0, .w = unused
}