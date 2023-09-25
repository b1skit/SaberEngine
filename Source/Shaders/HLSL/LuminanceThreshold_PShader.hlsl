// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "SaberCommon.hlsli"


float4 PShader(VertexOut In) : SV_Target
{
	// Sigmoid function tuning: https://www.desmos.com/calculator/w3hrskwpyb
	const float sigmoidRampPower = LuminanceThresholdParams.g_sigmoidParams.x;
	const float sigmoidSpeed = LuminanceThresholdParams.g_sigmoidParams.y;

	float3 input = Tex0.Sample(Clamp_Linear_Linear, In.UV0).rgb;
		
	const float maxChannel = max(input.x, max(input.y, input.z));
	float scale = pow(sigmoidSpeed * maxChannel, sigmoidRampPower);

	scale = isinf(scale) ? 10000.0f : scale; // Prevent NaNs from blowouts

	scale = scale / (scale + 1.f);

	return float4(input * scale, 1.f);
}