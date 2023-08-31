// © 2023 Adam Badke. All rights reserved.

#if !defined(BLUR_SHADER_LUMINANCE_THRESHOLD)
	
	
	// 5-tap Gaussian filter:
//	#define NUM_TAPS 5
//	#define TEXEL_OFFSET 2
//	uniform float weights[NUM_TAPS] = float[] (0.06136, 0.24477, 0.38774, 0.24477, 0.06136);		// 5 tap filter
//	uniform float weights[NUM_TAPS] = float[] (0.060626, 0.241843, 0.383103, 0.241843, 0.060626);	// Note: This is a 7 tap filter, but we ignore the outer 2 samples as they're only 0.00598

	// 9-tap Gaussian filter:
	#define NUM_TAPS 7
	#define TEXEL_OFFSET 3
	uniform float weights[NUM_TAPS] = float[] (0.005977, 0.060598, 0.241732, 0.382928, 0.241732, 0.060598, 0.005977);	// Note: This is a 9 tap filter, but we ignore the outer 2 samples as they're only 0.000229

//	// 11-tap Gaussian filter:
//	#define NUM_TAPS 9
//	#define TEXEL_OFFSET 4
//	uniform float weights[NUM_TAPS] = float[] (	0.000229, 0.005977, 0.060598, 0.24173, 0.382925, 0.24173, 0.060598, 0.005977, 0.000229);	// Note: This is a 11-tap filter, but we ignore the outer 2 samples as they're only 0.000003


#endif

// Pass 0: Blur luminance threshold:
#if defined(BLUR_SHADER_LUMINANCE_THRESHOLD)

	void main()
	{	
		// Sigmoid function tuning: https://www.desmos.com/calculator/w3hrskwpyb
		const float sigmoidRampPower = g_sigmoidParams.x;
		const float sigmoidSpeed = g_sigmoidParams.y;

		vec3 fragRGB = texture(Tex0, vOut.uv0.xy).rgb;
		
		const float maxChannel = max(fragRGB.x, max(fragRGB.y, fragRGB.z));
		float scale = pow(sigmoidSpeed * maxChannel, sigmoidRampPower);

		scale = isinf(scale) ? 10000.0 : scale; // Prevent NaNs from blowouts

		scale = scale / (scale + 1.0);

		fragRGB = fragRGB * scale;

		FragColor = vec4(fragRGB, 1.0);
	}


// Pass 1: Horizontal blur:
#elif defined(BLUR_SHADER_HORIZONTAL)

	void main()
	{
		vec3 total = vec3(0,0,0);
		vec2 uvs = vOut.uv0.xy;
		uvs.x -= g_bloomTargetResolution.z * TEXEL_OFFSET;	// Offset to the left

		for (int i = 0; i < NUM_TAPS; i++)
		{
			total += texture(Tex0, uvs).rgb * weights[i];

			uvs.x += g_bloomTargetResolution.z; // Move the sample right by 1 pixel
		}

		FragColor = vec4(total, 1.0);
	} 


// Pass 2: Vertical blur:
#elif defined(BLUR_SHADER_VERTICAL)
	
	void main()
	{	
		vec3 total = vec3(0,0,0);
		vec2 uvs = vOut.uv0.xy;
		uvs.y += g_bloomTargetResolution.w * TEXEL_OFFSET;	// Offset

		for (int i = 0; i < NUM_TAPS; i++)
		{
			total += texture(Tex0, uvs).rgb * weights[i];

			uvs.y -= g_bloomTargetResolution.w; // Move the sample down by 1 pixel
		}

		FragColor = vec4(total, 1);
	}

#endif
