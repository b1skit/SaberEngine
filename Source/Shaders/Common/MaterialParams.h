// © 2024 Adam Badke. All rights reserved.
#ifndef SE_MATERIAL_PARAMS
#define SE_MATERIAL_PARAMS

#include "PlatformConversions.h"


// GLTF metallic roughness PBR material parameter block
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-material
struct InstancedPBRMetallicRoughnessParamsData
{
	float4 g_baseColorFactor;

	float g_metallicFactor;
	float g_roughnessFactor;
	float g_normalScale;
	float g_occlusionStrength;

	// KHR_materials_emissive_strength: Multiplies emissive factor
	float4 g_emissiveFactorStrength; // .xyz = emissive factor, .w = emissive strength

	// Non-GLTF properties:
	float4 g_f0; // .xyz = f0, .w = unused. For non-metals only

	//float g_isDoubleSided;

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "InstancedPBRMetallicRoughnessParams";
#endif
};


#endif // SE_MATERIAL_PARAMS