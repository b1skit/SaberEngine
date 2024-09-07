// © 2023 Adam Badke. All rights reserved.
#define VOUT_COLOR
#define HAS_OWN_VERTEX_OUTPUT_DECLARATION
#define HAS_OWN_FRAGMENT_OUTPUT_DECLARATION
#include "SaberCommon.glsli"
#include "VertexStreams_PositionColor.glsli"


#if defined(SE_VERTEX_SHADER)
layout(location = 6) out VertexOut Out;
void VShader()
{
	Out.Color = Color;

	vec4 ndcPos = 
		_CameraParams.g_viewProjection * _InstancedTransformParams[gl_InstanceID].g_model * vec4(Position.xyz, 1.0);
	
	ndcPos.y *= -1.f; // Flip the Y axis in NDC space, as we're writing directly to the backbuffer

	gl_Position = ndcPos;
}
#endif


#if defined(SE_FRAGMENT_SHADER)
layout(location = 6) in VertexOut In;
layout (location = 0) out vec4 FragColor;
void PShader()
{
	FragColor = In.Color;
}
#endif