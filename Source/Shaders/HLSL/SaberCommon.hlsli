// � 2023 Adam Badke. All rights reserved.
#ifndef SABER_COMMON_HLSL
#define SABER_COMMON_HLSL

#include "CameraCommon.hlsli"


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

struct InstanceIndexParamsCB
{
	uint g_transformIdx;
	uint g_materialIdx;

	uint2 _padding;
};
StructuredBuffer<InstanceIndexParamsCB> InstanceIndexParams : register(t0, space0); // Indexed by instance ID


struct InstancedTransformParamsCB
{
	float4x4 g_model;
	float4x4 g_transposeInvModel;
};
StructuredBuffer<InstancedTransformParamsCB> InstancedTransformParams : register(t1, space0); // Indexed by instance ID


struct InstancedPBRMetallicRoughnessParamsCB
{
	float4 g_baseColorFactor;
	
	float g_metallicFactor;
	float g_roughnessFactor;
	float g_normalScale;
	float g_occlusionStrength;
	
	float4 g_emissiveFactorStrength; // .xyz = emissive factor, .w = emissive strength
	
	float4 g_f0; // .xyz = f0, .w = unused. For non-metals only
};
StructuredBuffer<InstancedPBRMetallicRoughnessParamsCB> InstancedPBRMetallicRoughnessParams : register(t2, space0);


struct IEMPMREMGenerationParamsCB
{
	float4 g_numSamplesRoughnessFaceIdx; // .x = numIEMSamples, .y = numPMREMSamples, .z = roughness, .w = faceIdx
	float4 g_mipLevelSrcWidthSrcHeightSrcNumMips; // .x = IEM mip level, .yz = src width/height, .w = src num mips
};
ConstantBuffer<IEMPMREMGenerationParamsCB> IEMPMREMGenerationParams;


struct BloomTargetParamsCB
{
	float4 g_bloomTargetResolution; // .x = width, .y = height, .z = 1/width, .w = 1/height
};
ConstantBuffer<BloomTargetParamsCB> BloomTargetParams;


struct LuminanceThresholdParamsCB
{
	float4 g_sigmoidParams; // .x = Sigmoid ramp power, .y = Sigmoid speed, .zw = unused
};
ConstantBuffer<LuminanceThresholdParamsCB> LuminanceThresholdParams;


struct GaussianBlurParamsCB
{
	float4 g_blurSettings; // .x = Bloom direction (0 = horizontal, 1 = vertical), .yzw = unused
};
ConstantBuffer<GaussianBlurParamsCB> GaussianBlurParams;


SamplerState Wrap_Linear_Linear;
SamplerState Clamp_Linear_Linear;
SamplerState Clamp_Nearest_Nearest;
SamplerState Clamp_LinearMipMapLinear_Linear;
SamplerState Wrap_LinearMipMapLinear_Linear;

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

#endif // SABER_COMMON_HLSL