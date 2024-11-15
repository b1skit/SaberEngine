// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_COMMON_HLSL
#define SABER_COMMON_HLSL

#include "CameraCommon.hlsli"
#include "Samplers.hlsli"


struct VertexOut
{
	float4 Position : SV_Position;
	
#ifdef VOUT_UV0
	float2 UV0		: TEXCOORD0;
#endif
	
#if MAX_UV_CHANNEL_IDX >= 1
	float2 UV1		: TEXCOORD1;
#endif
	
#ifdef VOUT_COLOR
	float4 Color	: COLOR0;
#endif	
#ifdef VOUT_LOCAL_POS
	float3 LocalPos : TEXCOORD5;
#endif
#ifdef VOUT_WORLD_POS
	float3 WorldPos : TEXCOORD6;
#endif
#ifdef VOUT_TBN
	float3x3 TBN	: TEXCOORD7;
#endif
#ifdef SABER_INSTANCING
	nointerpolation uint InstanceID : SV_InstanceID;
#endif
};


#endif // SABER_COMMON_HLSL