#version 460 core

#define SABER_VERTEX_SHADER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	gl_Position = g_viewProjection * g_model[gl_InstanceID] * vec4(in_position.xyz, 1.0);
	
	vOut.worldPos = (g_model[gl_InstanceID] * vec4(in_position.xyz, 1.0f)).xyz;
	vOut.uv0 = in_uv0;
	vOut.vertexColor = in_color;
	vOut.TBN = AssembleTBN(in_normal, in_tangent, g_model[gl_InstanceID]);
}