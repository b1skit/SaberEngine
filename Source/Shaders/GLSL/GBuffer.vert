// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	gl_Position = g_viewProjection * g_instancedMeshParams[gl_InstanceID].g_model * vec4(in_position.xyz, 1.0);
	
	vOut.uv0 = in_uv0;
	vOut.Color = in_color;
	vOut.TBN = BuildTBN(in_normal, in_tangent, g_instancedMeshParams[gl_InstanceID].g_transposeInvModel);
}