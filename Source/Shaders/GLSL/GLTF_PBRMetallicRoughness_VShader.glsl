// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN
#define VOUT_COLOR
#define SABER_INSTANCING
#include "NormalMapUtils.glsli"
#include "SaberCommon.glsli"
#include "VertexStreams_PosNmlTanUvCol.glsli"


void VShader()
{
	const uint transformIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_transformIdx;
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_materialIdx;

	const vec4 worldPos = _InstancedTransformParams[transformIdx].g_model * vec4(Position.xyz, 1.0);
	gl_Position = _CameraParams.g_viewProjection * worldPos;
	
#if defined(VOUT_WORLD_POS)
	Out.WorldPos = worldPos.xyz;
#endif

	Out.UV0 = UV0;

#if MAX_UV_CHANNEL_IDX >= 1
	Out.UV1 = UV1;
#endif

	Out.Color = _InstancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor * Color;

	Out.TBN = BuildTBN(Normal, Tangent, _InstancedTransformParams[transformIdx].g_transposeInvModel);
	
	InstanceParamsOut.InstanceID = gl_InstanceID;
}