// © 2023 Adam Badke. All rights reserved.
#version 460
#include "Color.glsli"
#include "SaberCommon.glsli"
#include "UVUtils.glsli"

layout(location=0, rgba32f) coherent uniform image2D output0;


float ComputeKarisAverageWeight(vec3 linearColor)
{
	const vec3 sRGBColor = LinearToSRGB(linearColor);
	const float luminance = sRGBToLuminance(sRGBColor);
	const float weight = 1.f / (1.f + luminance);
	return weight;
}


void BloomDown(in const uvec3 DTId)
{
	/* Based on the technique presented in "Next Generation Post Processing in Call of Duty Advanced Warfare", 
	*  SIGGRAPH 2014: https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
	*  We take the following 13 samples about the current pixel (g). Note: d, e, i, j are located at half-pixel offsets
	*
	*		a		b		c
	*			d		e
	*		f		g		h
	*			i		j
	*		k		l		m
	*/
	
	const ivec2 texelCoord = ivec2(DTId.xy);
	const uint srcMipLevel = uint(_BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.x);
	
	const vec2 pxOffset = _BloomComputeParams.g_dstTexDimensions.zw;
	const vec2 halfPxOffset = 0.5f * pxOffset;
	
	const uvec2 dstWidthHeight = uvec2(_BloomComputeParams.g_dstTexDimensions.xy);
	const vec2 uvs = PixelCoordsToScreenUV(DTId.xy, dstWidthHeight, vec2(0.5f, 0.5f), false);
	
	
	const vec2 uvA = uvs + vec2(-pxOffset.x, -pxOffset.y);
	const vec3 a = textureLod(Tex0, uvA, srcMipLevel).rgb;
	
	const vec2 uvB = uvs + vec2(0.f, -pxOffset.y);
	const vec3 b = textureLod(Tex0, uvB, srcMipLevel).rgb;
	
	const vec2 uvC = uvs + vec2(pxOffset.x, -pxOffset.y);
	const vec3 c = textureLod(Tex0, uvC, srcMipLevel).rgb;
	
	
	const vec2 uvD = uvs + vec2(-halfPxOffset.x, -halfPxOffset.y);
	const vec3 d = textureLod(Tex0, uvD, srcMipLevel).rgb;
	
	const vec2 uvE = uvs + vec2(halfPxOffset.x, -halfPxOffset.y);
	const vec3 e = textureLod(Tex0, uvE, srcMipLevel).rgb;
	
	
	const vec2 uvF = uvs + vec2(-pxOffset.x, 0.f);
	const vec3 f = textureLod(Tex0, uvF, srcMipLevel).rgb;
	
	const vec3 g = textureLod(Tex0, uvs, srcMipLevel).rgb;
	
	const vec2 uvH = uvs + vec2(pxOffset.x, 0.f);
	const vec3 h = textureLod(Tex0, uvH, srcMipLevel).rgb;
	
	
	const vec2 uvI = uvs + vec2(-halfPxOffset.x, halfPxOffset.y);
	const vec3 i = textureLod(Tex0, uvI, srcMipLevel).rgb;
	
	const vec2 uvJ = uvs + vec2(halfPxOffset.x, halfPxOffset.y);
	const vec3 j = textureLod(Tex0, uvJ, srcMipLevel).rgb;
	
	
	const vec2 uvK = uvs + vec2(-pxOffset.x, pxOffset.y);
	const vec3 k = textureLod(Tex0, uvK, srcMipLevel).rgb;
	
	const vec2 uvL = uvs + vec2(0.f, pxOffset.y);
	const vec3 l = textureLod(Tex0, uvL, srcMipLevel).rgb;
	
	const vec2 uvM = uvs + vec2(pxOffset.x, pxOffset.y);
	const vec3 m = textureLod(Tex0, uvM, srcMipLevel).rgb;
	
	/* We weight 4 blockS of 5 samples each centered at d, e, i, j like so:
	*	0.125		0.125
	*			0.5
	*	0.125		0.125
	* 
	* such that our weights sum to unity 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
	* Then, we average each of these 4 blocks of samples when we combine them, so the final weight for the contributions
	* from all 4 blocks sum to 1
	*/
	
	vec3 TL = (0.125f * 0.25f * (a + b + f + g));
	vec3 TR = (0.125f * 0.25f * (b + c + g + h));
	vec3 BL = (0.125f * 0.25f * (f + g + k + l));
	vec3 BR = (0.125f * 0.25f * (g + h + l + m));
	vec3 center = (d + e + i + j) * 0.5f * 0.25f;
	
	// Apply the Karis average to reduce fireflies/flickering:
	const float level = _BloomComputeParams.g_bloomRadiusWidthHeightLevelNumLevls.z;
	const bool deflickerEnabled = _BloomComputeParams.g_bloomDebug.x > 0.5f;
	if (level == 0 && deflickerEnabled)
	{
		TL *= ComputeKarisAverageWeight(TL);
		TR *= ComputeKarisAverageWeight(TR);
		BL *= ComputeKarisAverageWeight(BL);
		BR *= ComputeKarisAverageWeight(BR);
		center *= ComputeKarisAverageWeight(center);
	}

	imageStore(output0, texelCoord, vec4(TL + TR + BL + BR + center, 0.f));
}


void BloomUp(in const uvec3 DTId)
{	
	/* Combine the samples using a 3x3 tent filter:
	*	|	1	2	1	|
	*	|	2	4	2	| * 1/16
	*	|	1	2	1	|
	*/
	
	const ivec2 texelCoord = ivec2(DTId.xy);
	
	const uvec2 dstWidthHeight = uvec2(_BloomComputeParams.g_dstTexDimensions.xy);
	
	const uint srcMipLevel = uint(_BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.x);
	
	const vec2 uvs = PixelCoordsToScreenUV(DTId.xy, dstWidthHeight, vec2(0.5f, 0.5f), false);
	
	const vec2 bloomRadii = _BloomComputeParams.g_bloomRadiusWidthHeightLevelNumLevls.xy;
	const vec2 offset = vec2(_BloomComputeParams.g_dstTexDimensions.zw) * bloomRadii;
	
	
	const vec2 uvA = uvs + vec2(-offset.x, -offset.y);
	const vec3 a = textureLod(Tex0, uvA, srcMipLevel).rgb;
	
	const vec2 uvB = uvs + vec2(0.f, -offset.y);
	const vec3 b = textureLod(Tex0, uvB, srcMipLevel).rgb;
	
	const vec2 uvC = uvs + vec2(offset.x, -offset.y);
	const vec3 c = textureLod(Tex0, uvC, srcMipLevel).rgb;
	
	
	const vec2 uvD = uvs + vec2(-offset.x, 0.f);
	const vec3 d = textureLod(Tex0, uvD, srcMipLevel).rgb;
	
	const vec3 e = textureLod(Tex0, uvs, srcMipLevel).rgb;
	
	const vec2 uvF = uvs + vec2(offset.x, 0.f);
	const vec3 f = textureLod(Tex0, uvF, srcMipLevel).rgb;
	
	
	const vec2 uvG = uvs + vec2(-offset.x, offset.y);
	const vec3 g = textureLod(Tex0, uvG, srcMipLevel).rgb;
	
	const vec2 uvH = uvs + vec2(0.f, offset.y);
	const vec3 h = textureLod(Tex0, uvH, srcMipLevel).rgb;
	
	const vec2 uvI = uvs + vec2(offset.x, offset.y);
	const vec3 i = textureLod(Tex0, uvI, srcMipLevel).rgb;
	
	vec3 color = (a + c + g + i) * 1.f / 16.f;
	color += (b + d + f + h) * 2.f / 16.f;
	color += e * 4.f / 16.f;

	// Upsampling levels are in reverse order: <smallest mip> ...3, 2, 1 <largest mip>
	const uint level = uint(_BloomComputeParams.g_bloomRadiusWidthHeightLevelNumLevls.z);
	const uint numLevels = uint(_BloomComputeParams.g_bloomRadiusWidthHeightLevelNumLevls.w);
	if (level == numLevels) // First pass (smallest mip)
	{
		// First pass has no previous (correctly downsampled) mip to sample from
		imageStore(output0, texelCoord, vec4(color, 0.f));
	}
	else
	{
		// Progressive upscaling: We sum the current blurred result with the previous mip
		const uint prevSrcMip = srcMipLevel + 1;
		const vec3 prevBlurredMip = textureLod(Tex0, uvs, prevSrcMip).rgb;

		const float avgFactor = 0.5f;
		
		imageStore(output0, texelCoord, vec4(color + prevBlurredMip, 0.f) * avgFactor);
	}
}


void BilinearDown(in const uvec3 DTId)
{
	const ivec2 texelCoord = ivec2(DTId.xy);
	
	const uvec2 dstWidthHeight = uvec2(_BloomComputeParams.g_dstTexDimensions.xy);
	const uint srcMipLevel = uint(_BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.x);
	
	const vec2 uvs = PixelCoordsToScreenUV(DTId.xy, dstWidthHeight, vec2(0.5f, 0.5f), false);
	
	const vec4 color = textureLod(Tex0, uvs, srcMipLevel);

	imageStore(output0, texelCoord, color);
}


layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void CShader()
{
	const bool isDownStage = _BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.w > 0.5f;
	
	if (isDownStage)
	{
		const uint dstMipLevel = uint(_BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.y);
		const uint firstUpsampleSrcMipLevel = uint(_BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.z);
		if (dstMipLevel > firstUpsampleSrcMipLevel)
		{
			BilinearDown(gl_GlobalInvocationID);
		}
		else
		{
			BloomDown(gl_GlobalInvocationID);
		}		
	}
	else
	{
		BloomUp(gl_GlobalInvocationID);
	}
}