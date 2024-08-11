// © 2023 Adam Badke. All rights reserved.
#define VIN_NORMAL
#define VIN_TANGENT
#define VIN_COLOR
#define VIN_UV0
#define VOUT_TBN
#define VOUT_COLOR
#define SABER_INSTANCING
#include "NormalMapUtils.glsl"
#include "SaberCommon.glsl"


void main()
{
	const uint transformIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_transformIdx;
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_materialIdx;

	const vec4 worldPos = _InstancedTransformParams[transformIdx].g_model * vec4(Position.xyz, 1.0);
	gl_Position = _CameraParams.g_viewProjection * worldPos;
	
#if defined(VOUT_WORLD_POS)
	Out.WorldPos = worldPos.xyz;
#endif

	Out.UV0 = UV0;

	Out.Color = _InstancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor * Color;

	Out.TBN = BuildTBN(Normal, Tangent, _InstancedTransformParams[transformIdx].g_transposeInvModel);
	
	InstanceParamsOut.InstanceID = gl_InstanceID;
}