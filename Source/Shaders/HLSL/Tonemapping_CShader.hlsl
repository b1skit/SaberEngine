// © 2025 Adam Badke. All rights reserved.
#include "SaberComputeCommon.hlsli"

#include "CameraCommon.hlsli"
#include "Color.hlsli"
#include "Lighting.hlsli"
#include "Samplers.hlsli"
#include "UVUtils.hlsli"


RWTexture2D<float4> Lighting;
Texture2D<float4> Bloom;


// Attribution: 
// 2 different ACES implementations are used here:
// 1) By Krzysztof Narkowicz:
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
// 2) By Stephen Hill (@self_shadow), adapted from the BakingLab implementation here:
// https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl


// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
static const float3x3 ACESInputMat =
{
	{ 0.59719, 0.35458, 0.04823 },
	{ 0.07600, 0.90834, 0.01566 },
	{ 0.02840, 0.13383, 0.83777 }
};

// ODT_SAT => XYZ => D60_2_D65 => sRGB
static const float3x3 ACESOutputMat =
{
	{ 1.60475, -0.53108, -0.07367 },
	{ -0.10208, 1.10813, -0.00605 },
	{ -0.00327, -0.07276, 1.07602 }
};


float3 RRTAndODTFit(float3 v)
{
	float3 a = v * (v + 0.0245786f) - 0.000090537f;
	float3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return a / b;
}


float3 ACESFitted(float3 color)
{
	color = mul(ACESInputMat, color);

	// Apply RRT and ODT
	color = RRTAndODTFit(color);

	color = mul(ACESOutputMat, color);

	// Clamp to [0, 1]
	color = saturate(color);

	return color;
}


float3 ACESFilm(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}


float3 ApplyBloom(uint2 texelCoord, uint2 lightingWidthHeight)
{	
	const float2 uvs = PixelCoordsToScreenUV(texelCoord, lightingWidthHeight, float2(0.5f, 0.5f));
	
	const float3 color = Lighting[texelCoord].rgb;
	const float3 bloom = Bloom.SampleLevel(ClampMinMagMipLinear, uvs, 0).rgb;

	// Apply exposure:
	const float bloomExposure = CameraParams.g_bloomSettings.w;
	const float3 exposedBloom = ApplyExposure(bloom, bloomExposure);
	
	// TODO: Support auto exposure using the bottom mip of the bloom texture
	const float exposure = CameraParams.g_exposureProperties.x;
	const float3 exposedColor = ApplyExposure(color, exposure);
	
	// Blend the exposed bloom and scene color:
	const float bloomStrength = CameraParams.g_bloomSettings.x;
	const float3 blendedColor = lerp(exposedColor, exposedBloom, bloomStrength);
	
	return blendedColor;
}


[numthreads(8, 8, 1)]
void Tonemapping_ACES(ComputeIn In)
{
	const uint2 texelCoord = In.DTId.xy;
	
	uint2 lightingWidthHeight;
	Lighting.GetDimensions(lightingWidthHeight.x, lightingWidthHeight.y);
	
	if (texelCoord.x >= lightingWidthHeight.x || texelCoord.y >= lightingWidthHeight.y)
	{
		return;
	}
	
	const float3 blendedColor = ApplyBloom(texelCoord, lightingWidthHeight);
	
#if defined(ACES_FAST)
	const float3 toneMappedColor = ACESFilm(blendedColor);
#else
	const float3 toneMappedColor = ACESFitted(blendedColor);
#endif

	const float3 sRGBColor = LinearToSRGB(toneMappedColor);
	
	Lighting[texelCoord] = float4(sRGBColor, 1.f);
}


[numthreads(8, 8, 1)]
void Tonemapping_Reinhard(ComputeIn In)
{
	const uint2 texelCoord = In.DTId.xy;
	
	uint2 lightingWidthHeight;
	Lighting.GetDimensions(lightingWidthHeight.x, lightingWidthHeight.y);
	
	if (texelCoord.x >= lightingWidthHeight.x || texelCoord.y >= lightingWidthHeight.y)
	{
		return;
	}
	
	const float3 blendedColor = ApplyBloom(texelCoord, lightingWidthHeight);
	
	const float3 toneMappedColor = blendedColor / (blendedColor + float3(1.f, 1.f, 1.f));
	
	const float3 sRGBColor = LinearToSRGB(toneMappedColor);
	
	Lighting[texelCoord] = float4(sRGBColor, 1.f);
}