// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsli"
#include "VertexStreams_PositionUV.glsli"

#include "../Common/CameraParams.h"
#include "../Common/InstancingParams.h"

layout(binding=7) uniform CameraParams { CameraData _CameraParams; };

layout(std430, binding = 0) readonly buffer InstanceIndexParams { InstanceIndexData _InstanceIndexParams[]; };
layout(std430, binding = 1) readonly buffer TransformParams { TransformData _TransformParams[]; };


void VShader()
{
	const uint transformIdx = _InstanceIndexParams[gl_InstanceID].g_indexes.x;

	gl_Position = 
		_CameraParams.g_viewProjection * _TransformParams[transformIdx].g_model * vec4(Position.xyz, 1.0);

#if defined(SABER_INSTANCING)
	InstanceParamsOut.InstanceID = gl_InstanceID;
	
	// Not technically part of SABER_INSTANCING, but we only need this if we're executing a PShader
	Out.UV0 = UV0;
#endif
}