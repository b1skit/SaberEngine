#version 460 core

#define SABER_FRAGMENT_SHADER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"

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

	// Normal:
	gBuffer_out_worldNormal = vec4( WorldNormalFromTexture(MatNormal, data.uv0.xy, data.TBN), 0);

	// MatRMAO:
	gBuffer_out_RMAO		= texture(MatRMAO, data.uv0.xy);

	// Emissive:
	gBuffer_out_emissive	= texture(MatEmissive, data.uv0.xy) * emissiveIntensity;

	// Position:
	gBuffer_out_wPos	= vec4(data.worldPos.xyz, 1);

	// Material properties:
	gBuffer_out_matProp0	= MatProperty0;

	// Depth:
	gBuffer_out_depth		= vec4(gl_FragCoord.z, gl_FragCoord.z, gl_FragCoord.z, 1.0);	// Doesn't actually do anything...
}