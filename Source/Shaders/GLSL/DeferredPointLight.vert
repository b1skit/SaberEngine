// © 2023 Adam Badke. All rights reserved.
#define VOUT_WORLD_POS

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	gl_Position	= g_viewProjection * g_instancedMeshParams[gl_InstanceID].g_model * vec4(in_position, 1.0);
	vOut.worldPos = (g_instancedMeshParams[gl_InstanceID].g_model * vec4(in_position, 1.0f)).xyz;
}