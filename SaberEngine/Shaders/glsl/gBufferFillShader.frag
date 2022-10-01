#version 460 core

#define SABER_FRAGMENT_SHADER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"

// Built-in input variables:
// in vec4 gl_FragCoord; //  location of the fragment in window space. 
// in bool gl_FrontFacing;
// in vec2 gl_PointCoord;

// Note: Locations must match the order defined in material.h
layout (location = 0) out vec4 gBuffer_out_albedo;
layout (location = 1) out vec4 gBuffer_out_worldNormal;
layout (location = 2) out vec4 gBuffer_out_RMAO;
layout (location = 3) out vec4 gBuffer_out_emissive;
layout (location = 4) out vec4 gBuffer_out_wPos;
layout (location = 5) out vec4 gBuffer_out_matProp0;
layout (location = 6) out vec4 gBuffer_out_depth;

uniform float emissiveIntensity = 1.0;	// Overwritten during RenderManager.Initialize()


void main()
{
	// Albedo. Note: We use an sRGB-format texture, which converts this value from sRGB->linear space for free
	gBuffer_out_albedo = texture(MatAlbedo, data.uv0.xy) * g_baseColorFactor;

	// TODO: If a primitive specifies a vertex color using the attribute semantic property COLOR_0, then this value acts 
	// as an additional linear multiplier to base color.
	//https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material

	const vec3 texNormal = texture(MatNormal, data.uv0.xy).xyz;
	const vec3 worldNormal = WorldNormalFromTextureNormal(texNormal, data.TBN) * vec3(g_normalScale, g_normalScale, 1.0f);
	gBuffer_out_worldNormal = vec4(worldNormal, 0.0f);
	
	// Unpack/scale metallic/roughness: .G = roughness, .B = metallness
	const vec2 roughMetal = texture(MatMetallicRoughness, data.uv0.xy).gb * vec2(g_roughnessFactor, g_metallicFactor);

	// Unpack/scale AO:
	const float occlusion = texture(MatOcclusion, data.uv0.xy).r * g_occlusionStrength;
	//const float occlusion = clamp((1.0f + g_occlusionStrength) * (texture(MatOcclusion, data.uv0.xy).r - 1.0f), 0.0f, 1.0f);
	// TODO: GLTF specifies the above occlusion scaling, but CGLTF seems non-complicant & packs occlusion strength into
	// the texture scale value. For now, just use something sane.
	
	// Pack RMAO: 
	gBuffer_out_RMAO = vec4(roughMetal, occlusion, 1.0f);

	// Exposure:
	const float ev100 = GetEV100FromExposureSettings(CAM_APERTURE, CAM_SHUTTERSPEED, CAM_SENSITIVITY);
	const float exposure = Exposure(ev100);

	// Product of (emissiveTexture * emissiveFactor) is in cd/(m^2) (Candela per square meter)
	const vec3 emissive = texture(MatEmissive, data.uv0.xy).rgb * g_emissiveFactor;
	const float EC = 4.0; // EC == Exposure compensation. TODO: Make this user-controllable

	gBuffer_out_emissive = vec4(emissive * pow(2.0, ev100 + EC - 3.0) * exposure, 1.0f);

	gBuffer_out_wPos = vec4(data.worldPos.xyz, 1);

	// Material properties:
	gBuffer_out_matProp0 = vec4(g_f0, 1.0f);
}