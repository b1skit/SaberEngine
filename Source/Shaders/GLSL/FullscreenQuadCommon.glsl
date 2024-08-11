// © 2023 Adam Badke. All rights reserved.
#ifndef FULLSCREEN_QUAD_COMMON
#define FULLSCREEN_QUAD_COMMON

#define VIN_UV0
#include "SaberCommon.glsl"


void main()
{
	gl_Position	= vec4(Position, 1);	// Our screen aligned quad is already in clip space
	Out.UV0 = UV0;
}

#endif // FULLSCREEN_QUAD_COMMON