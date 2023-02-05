#version 460 core

#define SABER_FRAGMENT_SHADER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


// Note: Locations must match the order defined in material.h
layout (location = 0) out vec4 gBuffer_out_albedo;
layout (location = 1) out vec4 gBuffer_out_worldNormal;
layout (location = 2) out vec4 gBuffer_out_RMAO;
layout (location = 3) out vec4 gBuffer_out_emissive;
layout (location = 4) out vec4 gBuffer_out_wPos;
layout (location = 5) out vec4 gBuffer_out_matProp0;
layout (location = 6) out vec4 gBuffer_out_depth;


void main()
{
	// Albedo. Note: We use an sRGB-format texture, which converts this value from sRGB->linear space for free
	// g_baseColorFactor and vOut.vertexColor are factored into the albedo as per the GLTF 2.0 specifications
	gBuffer_out_albedo = texture(MatAlbedo, vOut.uv0.xy) * g_baseColorFactor * vOut.vertexColor;

	const vec3 texNormal = texture(MatNormal, vOut.uv0.xy).xyz;
	const vec3 worldNormal = WorldNormalFromTextureNormal(texNormal, vOut.TBN) * vec3(g_normalScale, g_normalScale, 1.0f);
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

	// Exposure:
	const float ev100 = GetEV100FromExposureSettings(CAM_APERTURE, CAM_SHUTTERSPEED, CAM_SENSITIVITY);
	const float exposure = Exposure(ev100);
	// TODO: Move this to a helper function (duplicated in deferredAmbientLightShader.frag)

	// Product of (emissiveTexture * emissiveFactor) is in cd/(m^2) (Candela per square meter)
	const vec3 emissive = texture(MatEmissive, vOut.uv0.xy).rgb * g_emissiveFactor * g_emissiveStrength;
	const float EC = 3.0; // EC == Exposure compensation. TODO: Make this user-controllable

	gBuffer_out_emissive = vec4(emissive * pow(2.0, ev100 + EC - 3.0) * exposure, 1.0f);

	gBuffer_out_wPos = vec4(vOut.worldPos.xyz, 1);

	// Material properties:
	gBuffer_out_matProp0 = vec4(g_f0, 1.0f);
}