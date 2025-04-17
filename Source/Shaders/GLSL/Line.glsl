// © 2023 Adam Badke. All rights reserved.
#define HAS_OWN_VERTEX_OUTPUT_DECLARATION
#define HAS_OWN_FRAGMENT_OUTPUT_DECLARATION

#include "SaberCommon.glsli"

#include "../Common/CameraParams.h"
#include "../Common/DebugParams.h"
#include "../Common/InstancingParams.h"

#if defined(SE_VERTEX_SHADER)

#if defined(DEBUG_NORMAL)
#include "../Generated/GLSL/VertexStreams_PositionNormal.glsli"
#elif defined(DEBUG_AXIS) || defined(DEBUG_WIREFRAME)
#include "../Generated/GLSL/VertexStreams_PositionOnly.glsli"
#else
#include "../Generated/GLSL/VertexStreams_PositionColor.glsli"
#endif

#endif // SE_VERTEX_SHADER


layout(binding=7) uniform CameraParams { CameraData _CameraParams; };

#if defined(DEBUG_WIREFRAME)
layout(binding=0) uniform InstanceIndexParams {	InstanceIndexData _InstanceIndexParams; };
#endif

// UBOs can't have a dynamic length; We use SSBOs for instancing instead
layout(std430, binding=1) readonly buffer InstancedTransformParams { TransformData _InstancedTransformParams[]; };


struct LineVertexOut
{
	vec4 Position;

#if !defined(DEBUG_NORMAL) && !defined(DEBUG_WIREFRAME)
	vec4 Color;
#endif
	
#ifdef DEBUG_NORMAL
	vec3 Normal;	
#endif
};

struct LineGeometryOut
{
	vec4 Position;

#if defined(DEBUG_AXIS)
	vec4 Color;
#endif
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
	InstanceID = gl_InstanceID;
		
#elif defined(DEBUG_AXIS)
	gl_Position = vec4(Position, 1.f);
	InstanceID = gl_InstanceID;

#else
	
#if defined(DEBUG_WIREFRAME)
	const uint transformIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_transformIdx;
#else
	const uint transformIdx = gl_InstanceID;
#endif

	vec4 ndcPos = 
		_CameraParams.g_viewProjection * _InstancedTransformParams[transformIdx].g_model * vec4(Position.xyz, 1.0);
	ndcPos.y *= -1.f; // Flip the Y axis in NDC space, as we're writing directly to the backbuffer

	gl_Position = ndcPos;

#if !defined(DEBUG_WIREFRAME)
	Out.Color = Color;
#endif
	
#endif

}
#endif


#if defined(SE_GEOMETRY_SHADER)

layout (points) in;

layout(location = 6) in LineVertexOut In[];
layout(location = 6) out LineGeometryOut Out;

flat in uint InstanceID[];


#if defined(DEBUG_NORMAL)
layout (line_strip, max_vertices = 2) out;
void GShader()
{
	const vec4 worldPos = _InstancedTransformParams[InstanceID[0]].g_model * gl_in[0].gl_Position;
	vec4 ndcPos = _CameraParams.g_viewProjection * worldPos;
	ndcPos.y *= -1.f; // Flip the Y axis in NDC space, as we're writing directly to the backbuffer

	gl_Position = ndcPos;
		
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


#if defined(DEBUG_AXIS)
layout (line_strip, max_vertices = 6) out;
void GShader()
{
	// Origin:		
	const vec4 worldPos = _InstancedTransformParams[InstanceID[0]].g_model * gl_in[0].gl_Position;
	vec4 ndcPos = _CameraParams.g_viewProjection * worldPos;
	ndcPos.y *= -1.f; // Flip the Y axis in NDC space, as we're writing directly to the backbuffer
	
	// Append the axis offsets:
	const float axisScale = _DebugParams.g_scales.y;
	const vec3 axisDirs[3] = { vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), vec3(0.f, 0.f, 1.f) };
	
	for (uint i = 0; i < 3; ++i)
	{	
		Out.Color = _DebugParams.g_colors[i];
		
		// Origin position:
		gl_Position = ndcPos;
		
		EmitVertex();
		
		// Axis offset:
		const vec4 offsetWorldPos = 
			_InstancedTransformParams[InstanceID[0]].g_model * vec4(gl_in[0].gl_Position.xyz + axisDirs[i], 1.f);

		const vec3 scaledOffsetDir = normalize(offsetWorldPos.xyz - worldPos.xyz) * axisScale;

		vec4 offsetNDCPos = _CameraParams.g_viewProjection * vec4(worldPos.xyz + scaledOffsetDir, 1.f);
		offsetNDCPos.y *= -1.f;

		gl_Position = offsetNDCPos;

		EmitVertex();

		// Prepare for the next line
		EndPrimitive();
	}
}
#endif


#endif


#if defined(SE_PIXEL_SHADER)

layout(location = 6) in LineVertexOut In;
layout (location = 0) out vec4 FragColor;

void PShader()
{
#if defined(DEBUG_NORMAL)
	FragColor = _DebugParams.g_colors[3];
#elif defined(DEBUG_WIREFRAME)
	FragColor = _DebugParams.g_colors[4];
#else
	FragColor = In.Color;
#endif
}
#endif