// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN
#define SABER_INSTANCING
#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	const uint transformIdx = g_instanceIndexes[gl_InstanceID].g_transformIdx;

	gl_Position = g_viewProjection * g_instancedTransformParams[transformIdx].g_model * vec4(in_position.xyz, 1.0);
	
	vOut.uv0 = in_uv0;
	vOut.Color = in_color;
	vOut.TBN = BuildTBN(in_normal, in_tangent, g_instancedTransformParams[transformIdx].g_transposeInvModel);
	
	InstanceID = gl_InstanceID;
}