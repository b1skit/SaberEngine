#version 430 core

#define SABER_VERTEX_SHADER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"

void main()
{
	gl_Position		= vec4(in_position, 1);	// Our screen aligned quad is already in clip space
	data.uv0		= in_uv0;
}