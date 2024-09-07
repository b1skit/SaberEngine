// © 2023 Adam Badke. All rights reserved.
#define VOUT_COLOR
#define VOUT_TBN
#define SABER_INSTANCING

#include "NormalMapUtils.glsli"
#include "SaberCommon.glsli"


layout (location = 0) out vec4 Albedo;
layout (location = 1) out vec4 WorldNormal;
layout (location = 2) out vec4 RMAOVn;
layout (location = 3) out vec4 Emissive;
layout (location = 4) out vec4 MatProp0Vn;


void PShader()
{
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[InstanceParamsIn.InstanceID].g_materialIdx;

	const vec4 matAlbedo = texture(MatAlbedo, In.UV0.xy);

	// Alpha clipping
	const float alphaCutoff = _InstancedPBRMetallicRoughnessParams[materialIdx].g_alphaCutoff.x;
	if (matAlbedo.a < alphaCutoff)
	{
		discard;
	}

	// Albedo. Note: We use an sRGB-format texture, which converts this value from sRGB->linear space for free
	// g_baseColorFactor and In.Color are factored into the albedo as per the GLTF 2.0 specifications
	const vec4 baseColorFactor = _InstancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor;
	Albedo = matAlbedo * baseColorFactor * In.Color;

	// Vertex normal:
	const vec3 vertexNormal = In.TBN[2];
	const vec2 encodedVertexNormal = EncodeOctohedralNormal(vertexNormal);

	// World-space normal:
	const float normalScaleFactor = _InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.z;
	const vec3 normalScale = vec3(normalScaleFactor, normalScaleFactor, 1.f); // Scales the normal's X, Y directions
	const vec3 texNormal = texture(MatNormal, In.UV0.xy).xyz;
	const vec3 worldNormal = WorldNormalFromTextureNormal(texNormal, In.TBN) * normalScale;

	WorldNormal = vec4(worldNormal, 0.0f);
	
	// Unpack/scale metallic/roughness: .G = roughness, .B = metallness
	const float metallicFactor = _InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.x;
	const float roughnessFactor = _InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.y;
	const vec2 roughMetal = texture(MatMetallicRoughness, In.UV0.xy).gb * vec2(roughnessFactor, metallicFactor);

	// Unpack/scale AO:
	const float occlusionStrength = _InstancedPBRMetallicRoughnessParams[materialIdx].g_metRoughNmlOccScales.w;
	const float occlusion = texture(MatOcclusion, In.UV0.xy).r * occlusionStrength;
	//const float occlusion = clamp((1.0f + g_occlusionStrength) * (texture(MatOcclusion, In.UV0.xy).r - 1.0f), 0.0f, 1.0f);
	// TODO: GLTF specifies the above occlusion scaling, but CGLTF seems non-compliant & packs occlusion strength into
	// the texture scale value. For now, just use something sane.
	
	// Pack RMAO: 
	RMAOVn = vec4(roughMetal, occlusion, encodedVertexNormal.x);

	// Emissive:
	const vec3 emissiveFactor = _InstancedPBRMetallicRoughnessParams[materialIdx].g_emissiveFactorStrength.rgb;
	const float emissiveStrength = _InstancedPBRMetallicRoughnessParams[materialIdx].g_emissiveFactorStrength.w;
	vec3 emissive = texture(MatEmissive, In.UV0.xy).rgb * emissiveFactor * emissiveStrength;

	Emissive = vec4(emissive, 1.0f);

	// Material properties:
	MatProp0Vn = vec4(
		_InstancedPBRMetallicRoughnessParams[materialIdx].g_f0.xyz,
		encodedVertexNormal.y);
}