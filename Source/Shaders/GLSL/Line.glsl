// © 2023 Adam Badke. All rights reserved.
#define HAS_OWN_VERTEX_OUTPUT_DECLARATION
#define HAS_OWN_FRAGMENT_OUTPUT_DECLARATION

#include "SaberCommon.glsli"

#include "../Common/DebugParams.h"


#if defined(SE_VERTEX_SHADER)

#if defined(DEBUG_NORMAL)
#include "../Generated/GLSL/VertexStreams_PositionNormal.glsli"
#else
#include "../Generated/GLSL/VertexStreams_PositionColor.glsli"
#endif

#endif // SE_VERTEX_SHADER

struct LineVertexOut
{
	vec4 Position;
	vec4 Color;
	
#ifdef DEBUG_NORMAL
	vec3 Normal;	
#endif
};

struct LineGeometryOut
{
	vec4 Position;
	vec4 Color;
};


layout(binding=16) uniform DebugParams { DebugData _DebugParams; };


#if defined(SE_VERTEX_SHADER)

layout(location = 6) out LineVertexOut Out;

flat out uint InstanceID;

void VShader()
{
#if defined(DEBUG_NORMAL)

	gl_Position = vec4(Position, 1.f);
	
	Out.Normal = Normal;
	
	Out.Color = _DebugParams.g_colors[3];
	
	InstanceID = gl_InstanceID;
		
#else
	
	Out.Color = Color;

	vec4 ndcPos = 
		_CameraParams.g_viewProjection * _InstancedTransformParams[gl_InstanceID].g_model * vec4(Position.xyz, 1.0);
	ndcPos.y *= -1.f; // Flip the Y axis in NDC space, as we're writing directly to the backbuffer

	gl_Position = ndcPos;
	
#endif
}
#endif


#if defined(SE_GEOMETRY_SHADER)

layout (points) in;
layout (line_strip, max_vertices = 2) out;

layout(location = 6) in LineVertexOut In[];
layout(location = 6) out LineGeometryOut Out;

flat in uint InstanceID[];

void GShader()
{
	const vec4 worldPos = _InstancedTransformParams[InstanceID[0]].g_model * gl_in[0].gl_Position;
	vec4 ndcPos = _CameraParams.g_viewProjection * worldPos;
	ndcPos.y *= -1.f; // Flip the Y axis in NDC space, as we're writing directly to the backbuffer

	gl_Position = ndcPos;
	Out.Color = In[0].Color;
		
	EmitVertex();

	// In[0], extended in the direction of the normal:
	const vec4 offsetWorldPos = 
		_InstancedTransformParams[InstanceID[0]].g_model * vec4(gl_in[0].gl_Position.xyz + In[0].Normal.xyz, 1.f);

	const vec3 scaledOffsetDir = normalize(offsetWorldPos.xyz - worldPos.xyz) * _DebugParams.g_scales.x;

	vec4 offsetNDCPos = _CameraParams.g_viewProjection * vec4(worldPos.xyz + scaledOffsetDir, 1.f);
	offsetNDCPos.y *= -1.f; 

	gl_Position = offsetNDCPos;

	EmitVertex();
}

#endif


#if defined(SE_FRAGMENT_SHADER)

layout(location = 6) in LineVertexOut In;
layout (location = 0) out vec4 FragColor;

void PShader()
{
	FragColor = In.Color;
}
#endif