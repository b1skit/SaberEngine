// Saber Engine GBuffer Shader. Fills GBuffer textures

#version 460 core

#define SABER_VERTEX_SHADER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	gl_Position	= g_viewProjection * in_model * vec4(in_position.xyz, 1.0);
	
	data.worldPos = (in_model * vec4(in_position.xyz, 1.0f)).xyz;

	data.uv0 = in_uv0;

	data.TBN = AssembleTBN(in_normal, in_tangent, in_model);
}