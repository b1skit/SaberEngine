// © 2022 Adam Badke. All rights reserved.

struct VertexPosColor
{
	float3 Position : POSITION;
	float3 Color	: COLOR;
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

// TODO: Rename as VShader
VertexShaderOutput main(VertexPosColor In)
{
	VertexShaderOutput Out;

	/*Out.Position = mul(ModelViewProjectionCB.MVP, float4(IN.Position, 1.0f));*/
	Out.Position = float4(In.Position.xyz, 0.5f);
	Out.Color = float4(In.Color, 1.0f);

	return Out;
}