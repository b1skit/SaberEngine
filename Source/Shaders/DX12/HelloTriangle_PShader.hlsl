// © 2022 Adam Badke. All rights reserved.


struct PixelShaderInput
{
	float4 Color	: COLOR;
};


float4 PShader(PixelShaderInput In) : SV_Target
{
	return In.Color;
}