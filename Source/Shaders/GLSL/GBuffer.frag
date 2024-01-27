// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN
#define SABER_INSTANCING

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
	const uint materialIdx = g_instanceIndexes[InstanceID].g_materialIdx;

	// Albedo. Note: We use an sRGB-format texture, which converts this value from sRGB->linear space for free
	// g_baseColorFactor and vOut.Color are factored into the albedo as per the GLTF 2.0 specifications
	const vec4 baseColorFactor = g_instancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor;
	gBuffer_out_albedo = texture(MatAlbedo, vOut.uv0.xy) * baseColorFactor * vOut.Color;

	// Alpha clipping:
	if (gBuffer_out_albedo.a < ALPHA_CUTOFF)
	{
		discard;
	}

	// World-space normal:
	const float normalScaleFactor = g_instancedPBRMetallicRoughnessParams[materialIdx].g_normalScale;
	const vec3 normalScale = vec3(normalScaleFactor, normalScaleFactor, 1.f); // Scales the normal's X, Y directions
	const vec3 texNormal = texture(MatNormal, vOut.uv0.xy).xyz;
	const vec3 worldNormal = WorldNormalFromTextureNormal(texNormal, vOut.TBN) * normalScale;

	gBuffer_out_worldNormal = vec4(worldNormal, 0.0f);
	
	// Unpack/scale metallic/roughness: .G = roughness, .B = metallness
	const float metallicFactor = g_instancedPBRMetallicRoughnessParams[materialIdx].g_metallicFactor;
	const float roughnessFactor = g_instancedPBRMetallicRoughnessParams[materialIdx].g_roughnessFactor;
	const vec2 roughMetal = texture(MatMetallicRoughness, vOut.uv0.xy).gb * vec2(roughnessFactor, metallicFactor);

	// Unpack/scale AO:
	const float occlusionStrength = g_instancedPBRMetallicRoughnessParams[materialIdx].g_occlusionStrength;
	const float occlusion = texture(MatOcclusion, vOut.uv0.xy).r * occlusionStrength;
	//const float occlusion = clamp((1.0f + g_occlusionStrength) * (texture(MatOcclusion, vOut.uv0.xy).r - 1.0f), 0.0f, 1.0f);
	// TODO: GLTF specifies the above occlusion scaling, but CGLTF seems non-complicant & packs occlusion strength into
	// the texture scale value. For now, just use something sane.
	
	// Pack RMAO: 
	gBuffer_out_RMAO = vec4(roughMetal, occlusion, 1.0f);

	// Emissive:
	const vec3 emissiveFactor = g_instancedPBRMetallicRoughnessParams[materialIdx].g_emissiveFactorStrength.rgb;
	const float emissiveStrength = g_instancedPBRMetallicRoughnessParams[materialIdx].g_emissiveFactorStrength.w;
	vec3 emissive = texture(MatEmissive, vOut.uv0.xy).rgb * emissiveFactor * emissiveStrength;

	gBuffer_out_emissive = vec4(emissive, 1.0f);

	// Material properties:
	gBuffer_out_matProp0 = g_instancedPBRMetallicRoughnessParams[materialIdx].g_f0; // .xyz = f0, .w = unused
}