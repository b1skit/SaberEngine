// © 2023 Adam Badke. All rights reserved.
#ifndef FULLSCREEN_QUAD_COMMON
#define FULLSCREEN_QUAD_COMMON

#include "SaberCommon.glsli"
#include "VertexStreams_PositionUV.glsli"


void VShader()
{
	gl_Position	= vec4(Position, 1);	// Our screen aligned quad is already in clip space

#if defined(FLIP_FULLSCREEN_QUAD_UVS)
	// SaberEngine uses a (0,0) = top left convention for UVs, which is non-default in OpenGL and results in the 
	// framebuffer being rendered upside down. We can flip the UVs here to get a correct image (e.g. for final stages)
	Out.UV0 = vec2(UV0.x, 1.f - UV0.y);
#else
	Out.UV0 = UV0;
#endif
	
}

#endif // FULLSCREEN_QUAD_COMMON