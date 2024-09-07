// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsli"
#include "VertexStreams_PositionOnly.glsli"


void VShader()
{
	const vec4 worldPos = _InstancedTransformParams[gl_InstanceID].g_model * vec4(Position, 1.0);
	gl_Position = _CameraParams.g_viewProjection * worldPos;
}