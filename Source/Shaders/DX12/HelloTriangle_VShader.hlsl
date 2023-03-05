// � 2022 Adam Badke. All rights reserved.

struct VertexPosColor
{
	float3 Position		: POSITION0;
	float3 Normal		: NORMAL0;
	float4 Tangent		: TANGENT0;
	float2 UV0			: TEXCOORD0;
	float4 Color		: COLOR0;
};


struct CameraParams
{
	matrix g_viewProjection;
};
ConstantBuffer<CameraParams> CameraParamsCB : register(b0);
// TODO: Replace this with a structure buffer


struct VertexShaderOutput
{
	float4 Color	: COLOR;
	float4 Position	: SV_Position;
};


VertexShaderOutput VShader(VertexPosColor In)
{
	VertexShaderOutput Out;

	Out.Position = mul(CameraParamsCB.g_viewProjection, float4(In.Position, 1.0f));
	Out.Color = In.Color;

	//Out.Color = float4(In.Normal, 1.f);
	//Out.Color = In.Tangent;
	//Out.Color = float4(In.UV0.xy, 0.f, 1.f);

	return Out;
}