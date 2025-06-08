// © 2023 Adam Badke. All rights reserved.
#define VOUT_COLOR
#include "SaberCommon.hlsli"

#include "../Common/DebugParams.h"
#include "../Common/InstancingParams.h"
#include "../Common/TransformParams.h"

#if defined(DEBUG_NORMAL)
#include "../_generated/HLSL/VertexStreams_PositionNormal.hlsli"
#elif defined(DEBUG_AXIS) || defined(DEBUG_WIREFRAME)
#include "../_generated/HLSL/VertexStreams_PositionOnly.hlsli"
#else
#include "../_generated/HLSL/VertexStreams_PositionColor.hlsli"
#endif


StructuredBuffer<InstanceIndexData> InstanceIndexParams : register(t0, space1);
StructuredBuffer<TransformData> TransformParams : register(t1, space1);


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
	
#if defined(VERTEXID_INSTANCING_LUT_IDX)
	const uint transformIdx = InstanceIndexParams[In.VertexID].g_indexes.x;
#elif defined(INSTANCEID_TRANSFORM_IDX)
	const uint transformIdx = In.InstanceID;
#else
	const uint transformIdx = InstanceIndexParams[In.InstanceID].g_indexes.x;
#endif
	
	const float4 worldPos = mul(TransformParams[NonUniformResourceIndex(transformIdx)].g_model, float4(In.Position, 1.f));
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

	const uint transformIdx = InstanceIndexParams[In[0].InstanceID].g_indexes.x;
		
	const float4 worldPos = mul(TransformParams[NonUniformResourceIndex(transformIdx)].g_model, In[0].Position);
	const float4 ndcPos = mul(CameraParams.g_viewProjection, worldPos);
	Out.Position = ndcPos;
		
	StreamOut.Append(Out);
		
	// In[0], extended in the direction of the normal:
	const float4 offsetWorldPos =
		mul(TransformParams[NonUniformResourceIndex(transformIdx)].g_model, float4(In[0].Position.xyz + In[0].Normal.xyz, 1.f));

	const float3 scaledOffsetDir = normalize(offsetWorldPos.xyz - worldPos.xyz) * DebugParams.g_scales.x;

	Out.Position = mul(CameraParams.g_viewProjection, float4(worldPos.xyz + scaledOffsetDir, 1.f));

	StreamOut.Append(Out);
}

#endif


#if defined(DEBUG_AXIS)
[maxvertexcount(6)]
void GShader(point LineVertexOut In[1], inout LineStream<LineGeometryOut> StreamOut)
{
	const uint transformIdx = InstanceIndexParams[In[0].InstanceID].g_indexes.x;

	// Origin:		
	const float4 worldPos = mul(TransformParams[NonUniformResourceIndex(transformIdx)].g_model, In[0].Position);
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
			mul(TransformParams[NonUniformResourceIndex(transformIdx)].g_model, float4(In[0].Position.xyz + axisDirs[i], 1.f));

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