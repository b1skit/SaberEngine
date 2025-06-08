// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsli"

#include "../Common/InstancingParams.h"
#include "../Common/MaterialParams.h"

layout(std430, binding = 0) readonly buffer InstanceIndexParams { InstanceIndexData _InstanceIndexParams[]; };
layout(std430, binding=2) readonly buffer PBRMetallicRoughnessParams {	PBRMetallicRoughnessData _PBRMetallicRoughnessParams[]; };

layout(binding=0) uniform sampler2D BaseColorTex;


void PShader()
{
	const uint instanceID = InstanceParamsIn.InstanceID;
	const uint materialIdx = _InstanceIndexParams[instanceID].g_indexes.y;

	const vec4 matAlbedo = texture(BaseColorTex, In.UV0.xy);

	// Alpha clipping
	const float alphaCutoff = _PBRMetallicRoughnessParams[materialIdx].g_f0AlphaCutoff.w;
	if (matAlbedo.a < alphaCutoff)
	{
		discard;
	}
}