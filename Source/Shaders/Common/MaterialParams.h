// © 2024 Adam Badke. All rights reserved.
#ifndef SE_MATERIAL_PARAMS
#define SE_MATERIAL_PARAMS

#include "PlatformConversions.h"


// GLTF metallic roughness PBR material buffer
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-material
struct PBRMetallicRoughnessData
{
	float4 g_baseColorFactor;

	// .x = metallic factor, .y = roughness factor, .z = normal scale, .w = occlusion strength
	float4 g_metRoughNmlOccScales;

	// KHR_materials_emissive_strength: Multiplies emissive factor
	float4 g_emissiveFactorStrength; // .xyz = emissive factor, .w = emissive strength

	float4 g_f0AlphaCutoff; // .xyz = f0 (non-metals only), .w = alpha cutoff

	uint4 g_uvChannelIndexes0;	// UV channel semantic index:  .xyzw = albedo, metallicRoughness, normal, occlusion
	uint4 g_uvChannelIndexes1;	// UV channel semantic index: .x = emissive, .yzw = unused

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "InstancedPBRMetallicRoughnessParams";
#endif
};


#endif // SE_MATERIAL_PARAMS