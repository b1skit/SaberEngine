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
layout(std430, binding = 0) readonly buffer InstanceIndexParams { InstanceIndexData _InstanceIndexParams[]; };

layout(std430, binding = 1) readonly buffer TransformParams { TransformData _TransformParams[]; };
layout(std430, binding = 2) readonly buffer PBRMetallicRoughnessParams { PBRMetallicRoughnessData _PBRMetallicRoughnessParams[]; };


void VShader()
{
	const uint transformIdx = _InstanceIndexParams[gl_InstanceID].g_indexes.x;
	const uint materialIdx = _InstanceIndexParams[gl_InstanceID].g_indexes.y;

	vec3 position = Position;
	
	const vec4 worldPos = _TransformParams[transformIdx].g_model * vec4(position, 1.0f);
	gl_Position = _CameraParams.g_viewProjection * worldPos;
	
#if defined(VOUT_WORLD_POS)
	Out.WorldPos = worldPos.xyz;
#endif

	Out.UV0 = UV0;

#if MAX_UV_CHANNEL_IDX >= 1
	Out.UV1 = UV1;
#endif

	Out.Color = Color; // Note: We apply the base color factor in the PShader

	Out.TBN = BuildTBN(Normal, Tangent, _TransformParams[transformIdx].g_transposeInvModel);
	
	InstanceParamsOut.InstanceID = gl_InstanceID;
}