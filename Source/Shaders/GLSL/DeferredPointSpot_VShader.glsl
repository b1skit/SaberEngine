// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsli"
#include "VertexStreams_PositionOnly.glsli"

#include "../Common/CameraParams.h"
#include "../Common/InstancingParams.h"


layout(binding=7) uniform CameraParams { CameraData _CameraParams; };

layout(std430, binding = 1) readonly buffer InstancedTransformParams { TransformData _InstancedTransformParams[]; };


void VShader()
{
	const vec4 worldPos = _InstancedTransformParams[gl_InstanceID].g_model * vec4(Position, 1.0);
	gl_Position = _CameraParams.g_viewProjection * worldPos;
}