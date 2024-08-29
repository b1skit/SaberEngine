// © 2023 Adam Badke. All rights reserved.
#version 460
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"

//#define NUM_TAPS 5
//#define NUM_TAPS 7
#define NUM_TAPS 9

#if NUM_TAPS == 5
// 7-tap filter, we ignore the outer 2 samples as they're only 0.00598
uniform float weights[NUM_TAPS] = float[] (
	0.060626f, 
	0.241843f, 
	0.383103f, 
	0.241843f, 
	0.060626f);	

#elif NUM_TAPS == 7

// 9-tap filter, we ignore the outer 2 samples as they're only 0.000229
uniform float weights[NUM_TAPS] = float[] (
	0.005977f, 
	0.060598f, 
	0.241732f, 
	0.382928f, 
	0.241732f, 
	0.060598f, 
	0.005977f);

#elif NUM_TAPS == 9

// 11-tap Gaussian filter:
// 11-tap filter, we ignore the outer 2 samples as they're only 0.000003
uniform float weights[NUM_TAPS] = float[] (
	0.000229f, 
	0.005977f, 
	0.060598f, 
	0.24173f, 
	0.382925f, 
	0.24173f, 
	0.060598f, 
	0.005977f, 
	0.000229f);

#endif


#define TEXEL_OFFSET (NUM_TAPS/2)


void PShader()
{
	const uvec2 directionSelection = uvec2(
		g_blurSettings.x < 0.5f ? 1 : 0, // g_blurSettings.x: 0 = horizontal, 1 = vertical
		g_blurSettings.x < 0.5f ? 0 : 1);

	const vec2 offset = 
		g_bloomTargetResolution.zw * directionSelection; // g_bloomTargetResolution .z = 1/xRes, .w = 1/yRes

	vec2 uvs = In.UV0.xy;
	uvs -= TEXEL_OFFSET * offset;

	vec3 total = vec3(0,0,0);

	for (int i = 0; i < NUM_TAPS; i++)
	{
		total += texture(Tex0, uvs).rgb * weights[i];

		uvs += offset;
	}

	FragColor = vec4(total, 1.0);
} 