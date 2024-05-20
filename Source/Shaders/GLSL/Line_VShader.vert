// © 2023 Adam Badke. All rights reserved.
#define VIN_COLOR
#define VOUT_COLOR
#include "SaberCommon.glsl"


void main()
{
	Out.Color = in_color;

	vec4 ndcPos = 
		_CameraParams.g_viewProjection * _InstancedTransformParams[gl_InstanceID].g_model * vec4(in_position.xyz, 1.0);
	
	ndcPos.y *= -1.f; // Flip the Y axis in NDC space, as we're writing directly to the backbuffer

	gl_Position = ndcPos;
}