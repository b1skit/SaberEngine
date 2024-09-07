// © 2024 Adam Badke. All rights reserved.
#define VOUT_LOCAL_POS
#include "SaberCommon.glsli"
#include "VertexStreams_PositionOnly.glsli"

void VShader()
{
	Out.LocalPos = Position; // Untransformed vertex position

	const mat4 rotView = mat4(mat3(_CameraParams.g_view)); // remove translation from the view matrix
	const vec4 clipPos = _CameraParams.g_projection * rotView * vec4(Position, 1.0);

	gl_Position = clipPos.xyww;
}