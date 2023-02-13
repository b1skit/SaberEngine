// © 2022 Adam Badke. All rights reserved.

struct PixelShaderInput
{
	float4 Color	: COLOR;
};

// TODO: Rename as PShader
float4 main(PixelShaderInput In) : SV_Target
{
	return In.Color;
}