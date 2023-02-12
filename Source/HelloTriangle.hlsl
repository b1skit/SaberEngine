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

VertexShaderOutput VShader(VertexPosColor In)
{
	VertexShaderOutput Out;

	/*Out.Position = mul(ModelViewProjectionCB.MVP, float4(IN.Position, 1.0f));*/
	Out.Position = In.Position;
	Out.Color = float4(In.Color, 1.0f);

	return Out;
}


struct PixelShaderInput
{
	float4 Color	: COLOR;
};

float4 PShader(PixelShaderInput In) : SV_Target
{
	return In.Color;
}