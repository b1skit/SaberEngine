#version 460 core

#define SABER_VERTEX_SHADER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	gl_Position	= g_viewProjection * g_model[gl_InstanceID] * vec4(in_position, 1.0);
	vOut.worldPos = (g_model[gl_InstanceID] * vec4(in_position, 1.0f)).xyz;
}