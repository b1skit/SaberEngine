// © 2024 Adam Badke. All rights reserved.
#define VIN_UV0
#include "SaberCommon.glsl"


void main()
{
	gl_Position = vec4(Position, 1); // Our screen aligned quad is already in clip space

	// NOTE: SaberEngine uses a (0,0) = top left convention for UVs. In OpenGL, this results in the framebuffer being
	// rendered upside down. So, we flip the UVs here to get a right-side-up image (as this is currently the final stage)
	Out.UV0 = vec2(UV0.x, 1.f - UV0.y);
}