// © 2023 Adam Badke. All rights reserved.
#define VOUT_LOCAL_POS

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	vOut.uv0 = in_uv0;
	vOut.localPos = in_position; // Untransformed vertex position

	mat4 rotView = mat4(mat3(g_view)); // remove translation from the view matrix
	vec4 clipPos = g_projection * rotView * vec4(in_position, 1.0);

	gl_Position = clipPos.xyww;
}