#version 460 core

#define SABER_VERTEX_SHADER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	gl_Position		= g_viewProjection * in_model * vec4(in_position.xyz, 1.0);

	data.worldPos	= (in_model * vec4(in_position.xyz, 1.0f)).xyz;
}