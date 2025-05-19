// © 2025 Adam Badke. All rights reserved.
#define VOUT_COLOR
#define SABER_INSTANCING
#include "SaberCommon.glsli"

#include "../Common/CameraParams.h"
#include "../Common/InstancingParams.h"
#include "../Common/MaterialParams.h"

#include "../Generated/GLSL/VertexStreams_PosUvCol.glsli"

layout(binding = 7) uniform CameraParams { CameraData _CameraParams; };
layout(std430, binding = 0) readonly buffer InstanceIndexParams { InstanceIndexData _InstanceIndexParams[]; };

layout(std430, binding = 1) readonly buffer InstancedTransformParams { TransformData _InstancedTransformParams[]; };
layout(std430, binding = 2) readonly buffer UnlitParams { UnlitData _UnlitParams[]; };


void VShader()
{
	const uint transformIdx = _InstanceIndexParams[gl_InstanceID].g_indexes.x;
	const uint materialIdx = _InstanceIndexParams[gl_InstanceID].g_indexes.y;

	vec3 position = Position;
	
	const vec4 worldPos = _InstancedTransformParams[transformIdx].g_model * vec4(position, 1.0f);
	gl_Position = _CameraParams.g_viewProjection * worldPos;
	
	Out.UV0 = UV0;

#if MAX_UV_CHANNEL_IDX >= 1
	Out.UV1 = UV1;
#endif

	Out.Color = _UnlitParams[materialIdx].g_baseColorFactor * Color;
	
	InstanceParamsOut.InstanceID = gl_InstanceID;
}