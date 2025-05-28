// © 2024 Adam Badke. All rights reserved.
#ifndef SE_MATERIAL_PARAMS
#define SE_MATERIAL_PARAMS

#include "PlatformConversions.h"

// gr::Material::MaterialID:
#define MAT_ID_GLTF_Unlit 0
#define MAT_ID_GLTF_PBRMetallicRoughness 1


// GLTF PBR metallic roughness material
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-material
struct PBRMetallicRoughnessData
{
	float4 g_baseColorFactor;

	// .x = metallic factor, .y = roughness factor, .z = normal scale, .w = occlusion strength
	float4 g_metRoughNmlOccScales;

	// KHR_materials_emissive_strength: Multiplies emissive factor
	float4 g_emissiveFactorStrength; // .xyz = emissive factor, .w = emissive strength

	float4 g_f0AlphaCutoff; // .xyz = f0 (non-metals only), .w = alpha cutoff

	uint4 g_uvChannelIndexes0;	// UV channel index: .xyzw = baseColor, metallicRoughness, normal, occlusion
	uint4 g_uvChannelIndexes1;	// UV channel index: .x = emissive, .y = MaterialID, .zw = unused

	// DX12 only:
	uint4 g_bindlessTextureIndexes0;	// .xyzw = BaseColor, MetallicRoughness, Normal, Occlusion
	uint4 g_bindlessTextureIndexes1;	// .x = Emissive, .yzw = unused

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "PBRMetallicRoughnessParams";
#endif
};


// GLTF Unlit material
//https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_materials_unlit
struct UnlitData
{
	float4 g_baseColorFactor;
	float4 g_alphaCutuff; // .x = alpha cutoff, .yzw = unused
	uint4 g_uvChannelIndexes0;	// .x = base color (& alpha) uv index, .y = MaterialID, .zw = unused

	// DX12 only:
	uint4 g_bindlessTextureIndexes0;	// .x = BaseColor, .yzw = unused

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "UnlitParams";
#endif
};


#endif // SE_MATERIAL_PARAMS