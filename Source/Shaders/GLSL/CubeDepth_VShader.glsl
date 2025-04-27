// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsli"
#include "VertexStreams_PositionUV.glsli"

#include "../Common/InstancingParams.h"

layout(std430, binding = 0) readonly buffer InstanceIndexParams { InstanceIndexData _InstanceIndexParams[]; };
layout(std430, binding = 1) readonly buffer InstancedTransformParams { TransformData _InstancedTransformParams[]; };


void VShader()
{
	const uint transformIdx = _InstanceIndexParams[gl_InstanceID].g_indexes.x;

	gl_Position = _InstancedTransformParams[transformIdx].g_model * vec4(Position.xyz, 1.0);

#if defined(SABER_INSTANCING)
	InstanceParamsOut.InstanceID = gl_InstanceID;

	// Not technically part of SABER_INSTANCING, but we only need this if we're executing a PShader
	Out.UV0 = UV0;
#endif

}