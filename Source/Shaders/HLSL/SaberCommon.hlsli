// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_COMMON_HLSL
#define SABER_COMMON_HLSL


#define ALPHA_CUTOFF 0.1f


struct VertexIn
{
	float3 Position : POSITION0;
	float3 Normal	: NORMAL0;
	float4 Tangent	: TANGENT0;
	float2 UV0		: TEXCOORD0;
	float4 Color	: COLOR0;
	
	uint InstanceID : SV_InstanceID;
};


struct VertexOut
{
	float4 Position : SV_Position;
	float2 UV0		: TEXCOORD0;
	float4 Color	: COLOR0;
	
#ifdef VOUT_TBN
	float3x3 TBN	: TEXCOORD1;
#endif
};


// Note: Aim for StructuredBuffers with sizes divisible by 128 bits = 16 bytes = sizeof(float4)

struct InstancedMeshParamsCB
{
	float4x4 g_model;
	float4x4 g_transposeInvModel;
};
StructuredBuffer<InstancedMeshParamsCB> InstancedMeshParams; // Indexed by instance ID


struct PBRMetallicRoughnessParamsCB
{
	float4 g_baseColorFactor;
	
	float g_metallicFactor;
	float g_roughnessFactor;
	float g_normalScale;
	float g_occlusionStrength;
	
	float4 g_emissiveFactorStrength; // .xyz = emissive factor, .w = emissive strength
	
	float4 g_f0; // .xyz = f0, .w = unused. For non-metals only
};
ConstantBuffer<PBRMetallicRoughnessParamsCB> PBRMetallicRoughnessParams;


struct CameraParamsCB
{
	float4x4 g_view;
	float4x4 g_invView;
	float4x4 g_projection;
	float4x4 g_invProjection;
	float4x4 g_viewProjection;
	float4x4 g_invViewProjection;

	float4 g_projectionParams; // .x = 1 (unused), .y = near, .z = far, .w = 1/far

	float4 g_exposureProperties; // .x = exposure, .y = ev100, .z = bloom exposure compensation

	float3 g_cameraWPos;
};
ConstantBuffer<CameraParamsCB> CameraParams;


SamplerState WrapLinearLinear;

Texture2D<float4> MatAlbedo; // TODO: Add a g_ prefix to texture names
Texture2D<float4> MatNormal;
Texture2D<float4> MatMetallicRoughness;
Texture2D<float4> MatOcclusion;
Texture2D<float4> MatEmissive;

#endif // SABER_COMMON_HLSL