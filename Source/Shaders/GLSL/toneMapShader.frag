#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


// Attribution: 
// 2 different ACES implementations are used here:
// 1) By Krzysztof Narkowicz:
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
// 2) By Stephen Hill (@self_shadow), adapted from the BakingLab implementation here:
// https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl


// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
uniform const mat3x3 ACESInputMat =
{
	{0.59719, 0.35458, 0.04823},
	{0.07600, 0.90834, 0.01566},
	{0.02840, 0.13383, 0.83777}
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
uniform const mat3x3 ACESOutputMat =
{
	{ 1.60475, -0.53108, -0.07367},
	{-0.10208,  1.10813, -0.00605},
	{-0.00327, -0.07276,  1.07602}
};


vec3 RRTAndODTFit(vec3 v)
{
	vec3 a = v * (v + 0.0245786f) - 0.000090537f;
	vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return a / b;
}


vec3 ACESFitted(vec3 color)
{
	color = ACESInputMat * color;

	// Apply RRT and ODT
	color = RRTAndODTFit(color);

	color = ACESOutputMat * color;

	// Clamp to [0, 1]
	color = clamp(color, 0.f, 1.f);

	return color;
}


vec3 ACESFilm(vec3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.f, 1.f);
}


//#define FAST_ACES

void main()
{	
	// NOTE: uv0.y was flipped in toneMapShader.vert to account for SaberEngine's use of a (0,0) top-left uv convention
	const vec3 color = texture(GBufferAlbedo, vOut.uv0.xy).rgb;

#if defined(FAST_ACES)
	vec3 toneMappedColor = ACESFilm(color);
#else
	vec3 toneMappedColor = ACESFitted(color);
#endif
	
	

	toneMappedColor = LinearToSRGB(toneMappedColor); // Apply gamma correction

	FragColor = vec4(toneMappedColor, 1.0);
} 