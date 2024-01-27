// © 2023 Adam Badke. All rights reserved.
#define SABER_DEPTH

#include "SaberCommon.glsl"


void main()
{
	const uint transformIdx = g_instanceIndexes[gl_InstanceID].g_transformIdx;

	gl_Position = g_instancedTransformParams[transformIdx].g_model * vec4(in_position.xyz, 1.0);
}