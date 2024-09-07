// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsli"
#include "VertexStreams_PositionUV.glsli"


void VShader()
{
	const uint transformIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_transformIdx;

	gl_Position = _InstancedTransformParams[transformIdx].g_model * vec4(Position.xyz, 1.0);

#if defined(SABER_INSTANCING)
	InstanceParamsOut.InstanceID = gl_InstanceID;

	// Not technically part of SABER_INSTANCING, but we only need this if we're executing a PShader
	Out.UV0 = UV0;
#endif

}