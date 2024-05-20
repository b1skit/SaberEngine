// © 2023 Adam Badke. All rights reserved.
#define VIN_UV0
#include "SaberCommon.glsl"


void main()
{
	const uint transformIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_transformIdx;

	gl_Position = 
		_CameraParams.g_viewProjection * _InstancedTransformParams[transformIdx].g_model * vec4(in_position.xyz, 1.0);

#if defined(SABER_INSTANCING)
	InstanceParamsOut.InstanceID = gl_InstanceID;
	
	// Not technically part of SABER_INSTANCING, but we only need this if we're executing a PShader
	Out.uv0 = in_uv0;
#endif
}