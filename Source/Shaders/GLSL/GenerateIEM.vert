#define VOUT_LOCAL_POS

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	vOut.localPos = in_position; // Untransformed vertex position

	const mat4 rotView = mat4(mat3(g_view)); // remove translation from the view matrix
	const vec4 clipPos = g_projection * rotView * vec4(in_position, 1.0);

	gl_Position = clipPos.xyww;
}