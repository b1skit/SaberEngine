// © 2023 Adam Badke. All rights reserved.
#ifndef FULLSCREEN_QUAD_COMMON
#define FULLSCREEN_QUAD_COMMON

#define VIN_UV0
#include "SaberCommon.glsl"


void main()
{
	gl_Position	= vec4(in_position, 1);	// Our screen aligned quad is already in clip space
	Out.uv0 = in_uv0;
}

#endif // FULLSCREEN_QUAD_COMMON