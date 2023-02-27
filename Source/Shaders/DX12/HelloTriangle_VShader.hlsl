// � 2022 Adam Badke. All rights reserved.

struct VertexPosColor
{
	float3 Position		: POSITION0;

	//float3 Normal		: NORMAL0;
	//float4 Tangent		: TANGENT0;
	//float2 UV0			: TEXCOORD0;

	float4 Color		: COLOR0;
};


struct ModelViewProjection
{
	matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VertexShaderOutput
{
	float4 Color	: COLOR;
	float4 Position	: SV_Position;
};

VertexShaderOutput VShader(VertexPosColor In)
{
	VertexShaderOutput Out;

	/*Out.Position = mul(ModelViewProjectionCB.MVP, float4(IN.Position, 1.0f));*/

	Out.Position = float4(In.Position.xyz, 1.0f);
	Out.Color = In.Color;

	return Out;
}