// � 2023 Adam Badke. All rights reserved.
#define SABER_DEPTH

#include "SaberCommon.glsl"


void main()
{
    gl_Position = g_instancedMeshParams[gl_InstanceID].g_model * vec4(in_position.xyz, 1.0);
}