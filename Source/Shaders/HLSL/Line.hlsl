// © 2023 Adam Badke. All rights reserved.
#define VOUT_COLOR
#include "SaberCommon.hlsli"

#include "../Common/DebugParams.h"

#if defined(DEBUG_NORMAL)
#include "../Generated/HLSL/VertexStreams_PositionNormal.hlsli"
#else
#include "../Generated/HLSL/VertexStreams_PositionColor.hlsli"
#endif


struct LineVertexOut
{
	float4 Position : SV_Position;
	float4 Color : COLOR0;
	
#ifdef DEBUG_NORMAL
	float3 Normal : NORMAL0;
	nointerpolation uint InstanceID : SV_InstanceID;
#endif
};

struct LineGeometryOut
{
	float4 Position : SV_Position;
	float4 Color : COLOR0;
};


ConstantBuffer<DebugData> DebugParams;


LineVertexOut VShader(VertexIn In)
{	
	LineVertexOut Out;

#if defined(DEBUG_NORMAL)
	Out.Position = float4(In.Position, 1.f);
	
	Out.Normal = In.Normal;
	
	Out.Color = DebugParams.g_colors[3];
	
	Out.InstanceID = In.InstanceID;
		
#else
	
	const float4 worldPos = mul(InstancedTransformParams[In.InstanceID].g_model, float4(In.Position, 1.f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	Out.Color = In.Color;
	
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
	Out.Color = In[0].Color;
		
	StreamOut.Append(Out);
		
	// In[0], extended in the direction of the normal:
	const float4 offsetWorldPos =
		mul(InstancedTransformParams[In[0].InstanceID].g_model, float4(In[0].Position.xyz + In[0].Normal.xyz, 1.f));

	const float3 scaledOffsetDir = normalize(offsetWorldPos.xyz - worldPos.xyz) * DebugParams.g_scales.x;

	Out.Position = mul(CameraParams.g_viewProjection, float4(worldPos.xyz + scaledOffsetDir, 1.f));

	StreamOut.Append(Out);
}

#endif


#if defined(DEBUG_NORMAL)
float4 PShader(LineGeometryOut In) : SV_Target
#else
float4 PShader(LineVertexOut In) : SV_Target
#endif
{
	return In.Color;
}