#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{	
	FragColor = texture(GBufferAlbedo, vOut.uv0.xy);
} 