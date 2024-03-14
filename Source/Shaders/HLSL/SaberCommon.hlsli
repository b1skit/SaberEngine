// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_COMMON_HLSL
#define SABER_COMMON_HLSL

#include "CameraCommon.hlsli"
#include "Samplers.hlsli"

#include "../Common/InstancingParams.h"
#include "../Common/MaterialParams.h"


#define ALPHA_CUTOFF 0.1f

struct VertexIn
{	
	float3 Position : SV_Position;
#ifdef VIN_NORMAL
	float3 Normal	: NORMAL0;
#endif
#ifdef VIN_TANGENT
	float4 Tangent	: TANGENT0;
#endif
#ifdef VIN_UV0
	float2 UV0		: TEXCOORD0;
#endif
#ifdef VIN_COLOR
	float4 Color	: COLOR0;
#endif
	
	uint InstanceID : SV_InstanceID;
};


struct VertexToGeometry
{
	float4 Position : SV_Position;
};


struct VertexOut
{
	float4 Position : SV_Position;
	
#ifdef VOUT_UV0
	float2 UV0		: TEXCOORD0;
#endif
#ifdef VOUT_COLOR
	float4 Color	: COLOR0;
#endif	
#ifdef VOUT_LOCAL_POS
	float3 LocalPos : TEXCOORD1;
#endif
#ifdef VOUT_WORLD_POS
	float3 WorldPos : TEXCOORD2;
#endif
#ifdef VOUT_TBN
	float3x3 TBN	: TEXCOORD3;
#endif
#ifdef VOUT_INSTANCE_ID
	nointerpolation uint InstanceID : SV_InstanceID;
#endif
};

// Note: Aim for StructuredBuffers with sizes divisible by 128 bits = 16 bytes = sizeof(float4)


StructuredBuffer<InstanceIndexData> InstanceIndexParams : register(t0, space0); // Indexed by instance ID
StructuredBuffer<InstancedTransformData> InstancedTransformParams : register(t1, space0); // Indexed by instance ID

StructuredBuffer<InstancedPBRMetallicRoughnessData> InstancedPBRMetallicRoughnessParams : register(t2, space0);


Texture2D<float4> MatAlbedo;
Texture2D<float4> MatNormal;
Texture2D<float4> MatMetallicRoughness;
Texture2D<float4> MatOcclusion;
Texture2D<float4> MatEmissive;

Texture2D<float4> GBufferAlbedo;
Texture2D<float4> GBufferWNormal;
Texture2D<float4> GBufferRMAO;
Texture2D<float4> GBufferEmissive;
Texture2D<float4> GBufferMatProp0;
Texture2D<float4> GBufferDepth;

Texture2D<float> Depth0;

Texture2D<float4> Tex0;
Texture2D<float4> Tex1;
Texture2D<float4> Tex2;
Texture2D<float4> Tex3;
Texture2D<float4> Tex4;
Texture2D<float4> Tex5;
Texture2D<float4> Tex6;
Texture2D<float4> Tex7;
Texture2D<float4> Tex8;

TextureCube<float4> CubeMap0;
TextureCube<float4> CubeMap1;

TextureCube<float> CubeDepth;

#endif // SABER_COMMON_HLSL