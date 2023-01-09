#version 460 core

#define SABER_VERTEX_SHADER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	gl_Position = vec4(in_position, 1); // Our screen aligned quad is already in clip space

	// NOTE: SaberEngine uses a (0,0) = top left convention for UVs. In OpenGL, this results in the framebuffer being
	// rendered upside down. So, we flip the UVs here to get a right-side-up image (as this is currently the final stage)
	vOut.uv0 = vec2(in_uv0.x, 1.f - in_uv0.y);
}