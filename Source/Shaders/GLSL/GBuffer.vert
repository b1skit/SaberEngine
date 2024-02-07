// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN
#define SABER_INSTANCING
#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	const uint transformIdx = g_instanceIndexes[gl_InstanceID].g_transformIdx;
	const uint materialIdx = g_instanceIndexes[gl_InstanceID].g_materialIdx;

	const vec4 worldPos = g_instancedTransformParams[transformIdx].g_model * vec4(in_position.xyz, 1.0);
	gl_Position = g_viewProjection * worldPos;
	
	vOut.uv0 = in_uv0;

	vOut.Color = g_instancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor * in_color;

	vOut.TBN = BuildTBN(in_normal, in_tangent, g_instancedTransformParams[transformIdx].g_transposeInvModel);
	
	InstanceID = gl_InstanceID;
}