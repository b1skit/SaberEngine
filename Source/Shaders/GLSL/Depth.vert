#define SABER_DEPTH

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{
	gl_Position = g_viewProjection * g_instancedMeshParams[gl_InstanceID].g_model * vec4(in_position.xyz, 1.0);
}