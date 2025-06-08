// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0

#include "SaberCommon.hlsli"

//#define NUM_TAPS 5
//#define NUM_TAPS 7
#define NUM_TAPS 9

#define TEXEL_OFFSET NUM_TAPS/2

Texture2D<float4> Tex0;


struct GaussianBlurParamsCB
{
	float4 g_blurSettings; // .x = Bloom direction (0 = horizontal, 1 = vertical), .yzw = unused
};
ConstantBuffer<GaussianBlurParamsCB> GaussianBlurParams;

struct BloomTargetParamsCB
{
	float4 g_bloomTargetResolution; // .x = width, .y = height, .z = 1/width, .w = 1/height
};
ConstantBuffer<BloomTargetParamsCB> BloomTargetParams;


float4 PShader(VertexOut In) : SV_Target
{
	#if NUM_TAPS == 5
// 7-tap filter, we ignore the outer 2 samples as they're only 0.00598
float weights[NUM_TAPS] =
{
	0.060626f, 
	0.241843f, 
	0.383103f, 
	0.241843f, 
	0.060626f
};

#elif NUM_TAPS == 7

// 9-tap filter, we ignore the outer 2 samples as they're only 0.000229
float weights[NUM_TAPS] =
{
	0.005977f, 
	0.060598f, 
	0.241732f, 
	0.382928f, 
	0.241732f, 
	0.060598f, 
	0.005977f
};

#elif NUM_TAPS == 9

// 11-tap Gaussian filter:
// 11-tap filter, we ignore the outer 2 samples as they're only 0.000003
float weights[NUM_TAPS] =
{
	0.000229f,
	0.005977f,
	0.060598f,
	0.24173f,
	0.382925f,
	0.24173f,
	0.060598f,
	0.005977f,
	0.000229f
};

#endif
	
	const uint2 directionSelection = uint2(
		GaussianBlurParams.g_blurSettings.x < 0.5f ? 1 : 0, // g_blurSettings.x: 0 = horizontal, 1 = vertical
		GaussianBlurParams.g_blurSettings.x < 0.5f ? 0 : 1);

	const float2 offset =
		BloomTargetParams.g_bloomTargetResolution.zw * directionSelection; // g_bloomTargetResolution .z = 1/xRes, .w = 1/yRes

	float2 uvs = In.UV0;
	uvs -= TEXEL_OFFSET * offset;

	float3 total = float3(0, 0, 0);
	for (int i = 0; i < NUM_TAPS; i++)
	{
		total += Tex0.Sample(ClampMinMagLinearMipPoint, uvs).rgb * weights[i];

		uvs += offset;
	}

	return float4(total, 1.0);
}