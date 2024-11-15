// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN
#define VOUT_COLOR
#define SABER_INSTANCING
#include "NormalMapUtils.glsli"
#include "SaberCommon.glsli"

#include "../Common/CameraParams.h"
#include "../Common/InstancingParams.h"
#include "../Common/MaterialParams.h"

#include "../Generated/GLSL/VertexStreams_PosNmlTanUvCol.glsli"

layout(binding=7) uniform CameraParams { CameraData _CameraParams; };
layout(binding=0) uniform InstanceIndexParams {	InstanceIndexData _InstanceIndexParams; };

layout(std430, binding=2) readonly buffer InstancedPBRMetallicRoughnessParams {	InstancedPBRMetallicRoughnessData _InstancedPBRMetallicRoughnessParams[]; };

// UBOs can't have a dynamic length; We use SSBOs for instancing instead
layout(std430, binding=1) readonly buffer InstancedTransformParams { InstancedTransformData _InstancedTransformParams[]; };


void VShader()
{
	const uint transformIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_transformIdx;
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_materialIdx;

	vec3 position = Position;
	
	const vec4 worldPos = _InstancedTransformParams[transformIdx].g_model * vec4(position, 1.0f);
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