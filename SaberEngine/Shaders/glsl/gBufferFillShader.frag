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
	gBuffer_out_albedo		= texture(MatAlbedo, data.uv0.xy);
	gBuffer_out_worldNormal = vec4( WorldNormalFromTexture(MatNormal, data.uv0.xy, data.TBN), 0);
	
	// Pack RMAO:
	gBuffer_out_RMAO = vec4(
						texture(MatMetallicRoughness, data.uv0.xy).gb, // G = roughness, B = metallness
						texture(MatOcclusion, data.uv0.xy).r,
						1.0f);

	const float ev100 = GetEV100FromExposureSettings(CAM_APERTURE, CAM_SHUTTERSPEED, CAM_SENSITIVITY);
	const float exposure = Exposure(ev100);

	const float EC = 4.0; // EC == Exposure compensation. TODO: Make this user-controllable
	const vec4 emissive = texture(MatEmissive, data.uv0.xy);
	gBuffer_out_emissive = vec4(emissive.rgb * pow(2.0, ev100 + EC - 3.0) * exposure, emissive.a);

	gBuffer_out_wPos		= vec4(data.worldPos.xyz, 1);

	// Material properties:
	gBuffer_out_matProp0	= MatProperty0;

	// Depth:
	gBuffer_out_depth		= vec4(gl_FragCoord.z, gl_FragCoord.z, gl_FragCoord.z, 1.0);	// Doesn't actually do anything...
}