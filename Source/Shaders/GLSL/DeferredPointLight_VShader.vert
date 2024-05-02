// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsl"


void main()
{
	const vec4 worldPos = _InstancedTransformParams[gl_InstanceID].g_model * vec4(in_position, 1.0);
	gl_Position = _CameraParams.g_viewProjection * worldPos;
}