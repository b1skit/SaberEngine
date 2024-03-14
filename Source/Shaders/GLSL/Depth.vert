// © 2023 Adam Badke. All rights reserved.
#define SABER_DEPTH

#include "SaberCommon.glsl"


void main()
{
	const uint transformIdx = _InstanceIndexParams[gl_InstanceID].g_transformIdx;

	gl_Position = 
		_CameraParams.g_viewProjection * _InstancedTransformParams[transformIdx].g_model * vec4(in_position.xyz, 1.0);
}