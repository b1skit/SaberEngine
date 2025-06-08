// © 2025 Adam Badke. All rights reserved.
#version 460 core
#define SABER_VEC4_OUTPUT
#define VOUT_COLOR
#define SABER_INSTANCING
#include "SaberCommon.glsli"

#include "UVUtils.glsli"

#include "../Common/CameraParams.h"
#include "../Common/InstancingParams.h"
#include "../Common/MaterialParams.h"


layout(binding = 0) uniform sampler2D BaseColorTex;

layout(std430, binding = 0) readonly buffer InstanceIndexParams { InstanceIndexData _InstanceIndexParams[]; };
layout(std430, binding = 2) readonly buffer UnlitParams { UnlitData _UnlitParams[]; };

layout(binding = 7) uniform CameraParams { CameraData _CameraParams; };


void PShader()
{
	const uint materialIdx = _InstanceIndexParams[InstanceParamsIn.InstanceID].g_indexes.y;

	const vec2 albedoUV = GetUV(In, _UnlitParams[materialIdx].g_uvChannelIndexes0.x);
	
	const vec4 matAlbedo = texture(BaseColorTex, albedoUV);
	const vec4 baseColorFactor = _UnlitParams[materialIdx].g_baseColorFactor;
	const vec4 linearAlbedo = matAlbedo * In.Color * baseColorFactor;

	FragColor = linearAlbedo;
}