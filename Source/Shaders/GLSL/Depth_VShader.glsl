// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsli"
#include "VertexStreams_PositionUV.glsli"

#include "../Common/CameraParams.h"
#include "../Common/InstancingParams.h"

layout(binding=7) uniform CameraParams { CameraData _CameraParams; };
layout(binding=0) uniform InstanceIndexParams {	InstanceIndexData _InstanceIndexParams; };

// UBOs can't have a dynamic length; We use SSBOs for instancing instead
layout(std430, binding=1) readonly buffer InstancedTransformParams { TransformData _InstancedTransformParams[]; };


void VShader()
{
	const uint transformIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_transformIdx;

	gl_Position = 
		_CameraParams.g_viewProjection * _InstancedTransformParams[transformIdx].g_model * vec4(Position.xyz, 1.0);

#if defined(SABER_INSTANCING)
	InstanceParamsOut.InstanceID = gl_InstanceID;
	
	// Not technically part of SABER_INSTANCING, but we only need this if we're executing a PShader
	Out.UV0 = UV0;
#endif
}