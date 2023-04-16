// © 2022 Adam Badke. All rights reserved.


struct VertexPosColor
{
	float3 Position		: POSITION0;
	float3 Normal		: NORMAL0;
	float4 Tangent		: TANGENT0;
	float2 UV0			: TEXCOORD0;
	float4 Color		: COLOR0;
};


// Note: Definitions made here are also visible in later related shader stages
struct CameraParamsCB
{
	float4x4 g_view;
	float4x4 g_invView;
	float4x4 g_projection;
	float4x4 g_invProjection;
	float4x4 g_viewProjection;
	float4x4 g_invViewProjection;

	float4 g_projectionParams; // .x = 1 (unused), .y = near, .z = far, .w = 1/far

	float3 g_cameraWPos;
};
ConstantBuffer<CameraParamsCB> CameraParams;
// TODO: Replace this with a structure buffer


struct VertexShaderOutput
{
	float4 Color	: COLOR;
	float4 Position	: SV_Position;
};


VertexShaderOutput VShader(VertexPosColor In)
{
	VertexShaderOutput Out;

	Out.Position = mul(CameraParams.g_viewProjection, float4(In.Position, 1.0f));
	Out.Color = In.Color;

	return Out;
}