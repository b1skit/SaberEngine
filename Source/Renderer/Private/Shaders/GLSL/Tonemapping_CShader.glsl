// © 2025 Adam Badke. All rights reserved.
#version 460

#include "Color.glsli"
#include "SaberCommon.glsli"
#include "Lighting.glsli"
#include "UVUtils.glsli"

#include "../Common/CameraParams.h"


layout(binding=7) uniform CameraParams { CameraData _CameraParams; };

layout(location = 0, rgba16f) coherent uniform image2D Lighting;
layout(binding = 1) uniform sampler2D Bloom;


// Attribution: 
// 2 different ACES implementations are used here:
// 1) By Krzysztof Narkowicz:
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
// 2) By Stephen Hill (@self_shadow), adapted from the BakingLab implementation here:
// https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl


// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
uniform const mat3x3 ACESInputMat =
{
	{0.59719, 0.07600, 0.02840},
	{0.35458, 0.90834, 0.13383},
	{0.04823, 0.01566, 0.83777}
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
uniform const mat3x3 ACESOutputMat =
{
	{ 1.60475, -0.10208, -0.00327},
	{-0.53108,  1.10813, -0.07276},
	{-0.07367, -0.00605,  1.07602}
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


vec3 ApplyBloom(uvec2 texelCoord, uint2 lightingWidthHeight)
{
	const vec2 uvs = PixelCoordsToScreenUV(texelCoord, vec2(lightingWidthHeight), vec2(0.5f, 0.5f), false);

	const vec3 color = imageLoad(Lighting, ivec2(texelCoord)).rgb;
	const vec3 bloom = texture(Bloom, uvs).rgb;

	// Apply exposure:
	const float bloomExposure = _CameraParams.g_bloomSettings.w;
	const vec3 exposedBloom = ApplyExposure(bloom, bloomExposure);	
	
	// TODO: Support auto exposure using the bottom mip of the bloom texture
	const float exposure = _CameraParams.g_exposureProperties.x;
	const vec3 exposedColor = ApplyExposure(color, exposure);
	
	// Blend the exposed bloom and scene color:
	const float bloomStrength = _CameraParams.g_bloomSettings.x;
	const vec3 blendedColor = mix(exposedColor, exposedBloom, bloomStrength);

	return blendedColor;
}


layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void Tonemapping_ACES()
{
	const uvec2 texelCoord = gl_GlobalInvocationID.xy;

	const ivec2 lightingWidthHeight = imageSize(Lighting);

	if (texelCoord.x >= lightingWidthHeight.x || texelCoord.y >= lightingWidthHeight.y)
	{
		return;
	}

	const vec3 blendedColor = ApplyBloom(texelCoord, lightingWidthHeight);
	
#if defined(FAST_ACES)
	const vec3 toneMappedColor = ACESFilm(blendedColor);
#else
	const vec3 toneMappedColor = ACESFitted(blendedColor);
#endif

	const vec3 sRGBColor = LinearToSRGB(toneMappedColor);

	imageStore(Lighting, ivec2(texelCoord), vec4(sRGBColor, 1.f));
}


layout (local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void Tonemapping_Reinhard()
{
	const uvec2 texelCoord = gl_GlobalInvocationID.xy;

	const ivec2 lightingWidthHeight = imageSize(Lighting);

	if (texelCoord.x >= lightingWidthHeight.x || texelCoord.y >= lightingWidthHeight.y)
	{
		return;
	}

	const vec3 blendedColor = ApplyBloom(texelCoord, lightingWidthHeight);
	
	const vec3 toneMappedColor = blendedColor / (blendedColor + vec3(1.f, 1.f, 1.f));
	
	const vec3 sRGBColor = LinearToSRGB(toneMappedColor);
	
	imageStore(Lighting, ivec2(texelCoord), vec4(sRGBColor, 1.f));
}