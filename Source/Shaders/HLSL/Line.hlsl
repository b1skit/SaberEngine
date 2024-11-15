// © 2023 Adam Badke. All rights reserved.
#define VOUT_COLOR
#include "SaberCommon.hlsli"

#include "../Common/DebugParams.h"
#include "../Common/InstancingParams.h"

#if defined(DEBUG_NORMAL)
#include "../Generated/HLSL/VertexStreams_PositionNormal.hlsli"
#elif defined(DEBUG_AXIS) || defined(DEBUG_WIREFRAME)
#include "../Generated/HLSL/VertexStreams_PositionOnly.hlsli"
#else
#include "../Generated/HLSL/VertexStreams_PositionColor.hlsli"
#endif

StructuredBuffer<InstancedTransformData> InstancedTransformParams : register(t0, space1); // Indexed by instance ID


struct LineVertexOut
{
	float4 Position : SV_Position;
	
#if !defined(DEBUG_NORMAL) && !defined(DEBUG_AXIS) && !defined(DEBUG_WIREFRAME)
	float4 Color : COLOR0;
#endif
	
#ifdef DEBUG_NORMAL
	float3 Normal : NORMAL0;
	nointerpolation uint InstanceID : SV_InstanceID;

#elif defined(DEBUG_AXIS)
	nointerpolation uint InstanceID : SV_InstanceID;
#endif
};

struct LineGeometryOut
{
	float4 Position : SV_Position;
	
#if defined(DEBUG_AXIS)
	float4 Color : COLOR0;
#endif
};


ConstantBuffer<DebugData> DebugParams;


LineVertexOut VShader(VertexIn In)
{	
	LineVertexOut Out;

#if defined(DEBUG_NORMAL)
	Out.Position = float4(In.Position, 1.f);
	Out.Normal = In.Normal;		
	Out.InstanceID = In.InstanceID;
	
#elif defined(DEBUG_AXIS)
	Out.Position = float4(In.Position, 1.f);		
	Out.InstanceID = In.InstanceID;
	
#else
	
	const float4 worldPos = mul(InstancedTransformParams[In.InstanceID].g_model, float4(In.Position, 1.f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
#if !defined(DEBUG_WIREFRAME)
	Out.Color = In.Color;
#endif
	
#endif
	
	return Out;
}


#if defined(DEBUG_NORMAL)
[maxvertexcount(2)]
void GShader(point LineVertexOut In[1], inout LineStream<LineGeometryOut> StreamOut)
{
	LineGeometryOut Out;
		
	const float4 worldPos = mul(InstancedTransformParams[In[0].InstanceID].g_model, In[0].Position);
	const float4 ndcPos = mul(CameraParams.g_viewProjection, worldPos);
	Out.Position = ndcPos;
		
	StreamOut.Append(Out);
		
	// In[0], extended in the direction of the normal:
	const float4 offsetWorldPos =
		mul(InstancedTransformParams[In[0].InstanceID].g_model, float4(In[0].Position.xyz + In[0].Normal.xyz, 1.f));

	const float3 scaledOffsetDir = normalize(offsetWorldPos.xyz - worldPos.xyz) * DebugParams.g_scales.x;

	Out.Position = mul(CameraParams.g_viewProjection, float4(worldPos.xyz + scaledOffsetDir, 1.f));

	StreamOut.Append(Out);
}

#endif


#if defined(DEBUG_AXIS)
[maxvertexcount(6)]
void GShader(point LineVertexOut In[1], inout LineStream<LineGeometryOut> StreamOut)
{
	// Origin:		
	const float4 worldPos = mul(InstancedTransformParams[In[0].InstanceID].g_model, In[0].Position);
	const float4 ndcPos = mul(CameraParams.g_viewProjection, worldPos);
	
	// Append the axis offsets:
	const float axisScale = DebugParams.g_scales.y;
	const float3 axisDirs[3] = { float3(1.f, 0.f, 0.f), float3(0.f, 1.f, 0.f), float3(0.f, 0.f, 1.f) };
	
	[unroll(3)]
	for (uint i = 0; i < 3; ++i)
	{
		LineGeometryOut Out;
		
		Out.Color = DebugParams.g_colors[i];
		
		// Origin position:
		Out.Position = ndcPos;
		
		StreamOut.Append(Out);
		
		// Axis offset:
		const float4 offsetWorldPos =
			mul(InstancedTransformParams[In[0].InstanceID].g_model, float4(In[0].Position.xyz + axisDirs[i], 1.f));

		const float3 scaledOffsetDir = normalize(offsetWorldPos.xyz - worldPos.xyz) * axisScale;

		Out.Position = mul(CameraParams.g_viewProjection, float4(worldPos.xyz + scaledOffsetDir, 1.f));
		Out.Color = DebugParams.g_colors[i];
		
		StreamOut.Append(Out);

		// Prepare for the next line
		StreamOut.RestartStrip();
	}
}


#endif


#if defined(DEBUG_NORMAL) || defined(DEBUG_AXIS)
float4 PShader(LineGeometryOut In) : SV_Target
#else
float4 PShader(LineVertexOut In) : SV_Target
#endif
{
#if defined(DEBUG_NORMAL)
	return DebugParams.g_colors[3];
#elif defined(DEBUG_WIREFRAME)
	return DebugParams.g_colors[4];
#else
	return In.Color;
#endif
}