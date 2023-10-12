// � 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	const vec4 worldPos = g_instancedMeshParams[gl_InstanceID].g_model * vec4(in_position, 1.0);
	gl_Position = g_viewProjection * worldPos;
}