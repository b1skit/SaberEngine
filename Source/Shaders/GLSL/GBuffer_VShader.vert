// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN
#define SABER_INSTANCING
#include "NormalMapUtils.glsl"
#include "SaberCommon.glsl"


void main()
{
	const uint transformIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_transformIdx;
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_materialIdx;

	const vec4 worldPos = _InstancedTransformParams[transformIdx].g_model * vec4(in_position.xyz, 1.0);
	gl_Position = _CameraParams.g_viewProjection * worldPos;
	
	vOut.uv0 = in_uv0;

	vOut.Color = _InstancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor * in_color;

	vOut.TBN = BuildTBN(in_normal, in_tangent, _InstancedTransformParams[transformIdx].g_transposeInvModel);
	
	InstanceID = gl_InstanceID;
}