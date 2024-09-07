// © 2023 Adam Badke. All rights reserved.
#define VOUT_COLOR
#include "SaberCommon.glsli"
#include "VertexStreams_PositionColor.glsli"


void VShader()
{
	Out.Color = Color;

	vec4 ndcPos = 
		_CameraParams.g_viewProjection * _InstancedTransformParams[gl_InstanceID].g_model * vec4(Position.xyz, 1.0);
	
	ndcPos.y *= -1.f; // Flip the Y axis in NDC space, as we're writing directly to the backbuffer

	gl_Position = ndcPos;
}