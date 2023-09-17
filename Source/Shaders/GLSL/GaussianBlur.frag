// © 2023 Adam Badke. All rights reserved.
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"

// 5-tap Gaussian filter:
//	#define NUM_TAPS 5
//	#define TEXEL_OFFSET 2
//	uniform float weights[NUM_TAPS] = float[] (0.06136, 0.24477, 0.38774, 0.24477, 0.06136);		// 5 tap filter
//	uniform float weights[NUM_TAPS] = float[] (0.060626, 0.241843, 0.383103, 0.241843, 0.060626);	// Note: This is a 7 tap filter, but we ignore the outer 2 samples as they're only 0.00598

//// 9-tap Gaussian filter:
//// Note: This is a 9 tap filter, but we ignore the outer 2 samples as they're only 0.000229
//#define NUM_TAPS 7
//#define TEXEL_OFFSET 3
//uniform float weights[NUM_TAPS] = float[] (0.005977, 0.060598, 0.241732, 0.382928, 0.241732, 0.060598, 0.005977);	

// 11-tap Gaussian filter:
// Note: This is a 11-tap filter, but we ignore the outer 2 samples as they're only 0.000003
#define NUM_TAPS 9
#define TEXEL_OFFSET 4
uniform float weights[NUM_TAPS] = float[] (
	0.000229, 
	0.005977, 
	0.060598, 
	0.24173, 
	0.382925, 
	0.24173, 
	0.060598, 
	0.005977, 
	0.000229);


void main()
{
	const uvec2 directionSelection = uvec2(
		g_blurSettings.x < 0.5f ? 1 : 0, // g_blurSettings.x: 0 = horizontal, 1 = vertical
		g_blurSettings.x < 0.5f ? 0 : 1);

	const vec2 offset = 
		g_bloomTargetResolution.zw * directionSelection; // g_bloomTargetResolution .z = 1/xRes, .w = 1/yRes

	vec2 uvs = vOut.uv0.xy;
	uvs -= TEXEL_OFFSET * offset;

	vec3 total = vec3(0,0,0);

	for (int i = 0; i < NUM_TAPS; i++)
	{
		total += texture(Tex0, uvs).rgb * weights[i];

		uvs += offset;
	}

	FragColor = vec4(total, 1.0);
} 