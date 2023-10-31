// � 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsl"


void main()
{
	vOut.Color = in_color;

	vec4 ndcPos = g_viewProjection * g_instancedMeshParams[gl_InstanceID].g_model * vec4(in_position.xyz, 1.0);
	ndcPos.y *= -1.f; // Flip the Y axis in NDC space

	gl_Position = ndcPos;
}