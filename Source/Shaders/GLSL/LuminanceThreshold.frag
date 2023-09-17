#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


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