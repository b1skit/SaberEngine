// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsli"
#include "VertexStreams_PositionOnly.glsli"

#include "../Common/CameraParams.h"
#include "../Common/InstancingParams.h"
#include "../Common/TransformParams.h"


layout(binding=7) uniform CameraParams { CameraData _CameraParams; };

layout(std430, binding = 0) readonly buffer InstanceIndexParams { InstanceIndexData _InstanceIndexParams[]; };
layout(std430, binding = 1) readonly buffer TransformParams { TransformData _TransformParams[]; };


void VShader()
{
	const uint transformIdx = _InstanceIndexParams[gl_InstanceID].g_indexes.x;

	const vec4 worldPos = _TransformParams[transformIdx].g_model * vec4(Position, 1.0);
	gl_Position = _CameraParams.g_viewProjection * worldPos;
}