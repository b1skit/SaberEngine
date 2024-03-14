// © 2023 Adam Badke. All rights reserved.
#include "BloomCommon.hlsli"
#include "SaberComputeCommon.hlsli"
#include "Color.hlsli"
#include "UVUtils.hlsli"

#include "../Common/BloomComputeParams.h"


SamplerState ClampMinMagMipLinear;
Texture2D<float4> Tex0;
ConstantBuffer<BloomComputeData> BloomComputeParams;


void BloomDown(ComputeIn In)
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
	
	const uint2 texelCoord = In.DTId.xy;
	static const uint srcMipLevel = BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.x;
	
	static const float2 pxOffset = BloomComputeParams.g_dstTexDimensions.zw;
	static const float2 halfPxOffset = 0.5f * pxOffset;
	
	static const uint2 dstWidthHeight = uint2(BloomComputeParams.g_dstTexDimensions.xy);
	const float2 uvs = PixelCoordsToUV(In.DTId.xy, dstWidthHeight, float2(0.5f, 0.5f));
	
	
	const float2 uvA = uvs + float2(-pxOffset.x, -pxOffset.y);
	const float3 a = Tex0.SampleLevel(ClampMinMagMipLinear, uvA, srcMipLevel).rgb;
	
	const float2 uvB = uvs + float2(0.f, -pxOffset.y);
	const float3 b = Tex0.SampleLevel(ClampMinMagMipLinear, uvB, srcMipLevel).rgb;
	
	const float2 uvC = uvs + float2(pxOffset.x, -pxOffset.y);
	const float3 c = Tex0.SampleLevel(ClampMinMagMipLinear, uvC, srcMipLevel).rgb;
	
	
	const float2 uvD = uvs + float2(-halfPxOffset.x, -halfPxOffset.y);
	const float3 d = Tex0.SampleLevel(ClampMinMagMipLinear, uvD, srcMipLevel).rgb;
	
	const float2 uvE = uvs + float2(halfPxOffset.x, -halfPxOffset.y);
	const float3 e = Tex0.SampleLevel(ClampMinMagMipLinear, uvE, srcMipLevel).rgb;
	
	
	const float2 uvF = uvs + float2(-pxOffset.x, 0.f);
	const float3 f = Tex0.SampleLevel(ClampMinMagMipLinear, uvF, srcMipLevel).rgb;
	
	const float3 g = Tex0.SampleLevel(ClampMinMagMipLinear, uvs, srcMipLevel).rgb;
	
	const float2 uvH = uvs + float2(pxOffset.x, 0.f);
	const float3 h = Tex0.SampleLevel(ClampMinMagMipLinear, uvH, srcMipLevel).rgb;
	
	
	const float2 uvI = uvs + float2(-halfPxOffset.x, halfPxOffset.y);
	const float3 i = Tex0.SampleLevel(ClampMinMagMipLinear, uvI, srcMipLevel).rgb;
	
	const float2 uvJ = uvs + float2(halfPxOffset.x, halfPxOffset.y);
	const float3 j = Tex0.SampleLevel(ClampMinMagMipLinear, uvJ, srcMipLevel).rgb;
	
	
	const float2 uvK = uvs + float2(-pxOffset.x, pxOffset.y);
	const float3 k = Tex0.SampleLevel(ClampMinMagMipLinear, uvK, srcMipLevel).rgb;
	
	const float2 uvL = uvs + float2(0.f, pxOffset.y);
	const float3 l = Tex0.SampleLevel(ClampMinMagMipLinear, uvL, srcMipLevel).rgb;
	
	const float2 uvM = uvs + float2(pxOffset.x, pxOffset.y);
	const float3 m = Tex0.SampleLevel(ClampMinMagMipLinear, uvM, srcMipLevel).rgb;
	
	/* We weight 4 blockS of 5 samples each centered at d, e, i, j like so:
	*	0.125		0.125
	*			0.5
	*	0.125		0.125
	* 
	* such that our weights sum to unity 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
	* Then, we average each of these 4 blocks of samples when we combine them, so the final weight for the contributions
	* from all 4 blocks sum to 1
	*/
	
	float3 TL = (0.125f * 0.25f * (a + b + f + g));
	float3 TR = (0.125f * 0.25f * (b + c + g + h));
	float3 BL = (0.125f * 0.25f * (f + g + k + l));
	float3 BR = (0.125f * 0.25f * (g + h + l + m));
	float3 center = (d + e + i + j) * 0.5f * 0.25f;
	
	// Apply the Karis average to reduce fireflies/flickering:
	static const float level = BloomComputeParams.g_bloomRadiusWidthHeightLevelNumLevls.z;
	static const bool deflickerEnabled = BloomComputeParams.g_bloomDebug.x > 0.5f;
	if (level == 0 && deflickerEnabled)
	{
		TL *= ComputeKarisAverageWeight(TL);
		TR *= ComputeKarisAverageWeight(TR);
		BL *= ComputeKarisAverageWeight(BL);
		BR *= ComputeKarisAverageWeight(BR);
		center *= ComputeKarisAverageWeight(center);
	}
	
	output0[texelCoord] = float4(TL + TR + BL + BR + center, 0.f);
}


void BloomUp(ComputeIn In)
{	
	/* Combine the samples using a 3x3 tent filter:
	*	|	1	2	1	|
	*	|	2	4	2	| * 1/16
	*	|	1	2	1	|
	*/
	
	const uint2 texelCoord = In.DTId.xy;
	
	const uint2 dstWidthHeight = uint2(BloomComputeParams.g_dstTexDimensions.xy);
	
	const uint srcMipLevel = BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.x;
	
	const float2 uvs = PixelCoordsToUV(In.DTId.xy, dstWidthHeight, float2(0.5f, 0.5f));
	
	const float2 bloomRadii = BloomComputeParams.g_bloomRadiusWidthHeightLevelNumLevls.xy;
	const float2 offset = float2(BloomComputeParams.g_dstTexDimensions.zw) * bloomRadii;
	
	
	const float2 uvA = uvs + float2(-offset.x, -offset.y);
	const float3 a = Tex0.SampleLevel(ClampMinMagMipLinear, uvA, srcMipLevel).rgb;
	
	const float2 uvB = uvs + float2(0.f, -offset.y);
	const float3 b = Tex0.SampleLevel(ClampMinMagMipLinear, uvB, srcMipLevel).rgb;
	
	const float2 uvC = uvs + float2(offset.x, -offset.y);
	const float3 c = Tex0.SampleLevel(ClampMinMagMipLinear, uvC, srcMipLevel).rgb;
	
	
	const float2 uvD = uvs + float2(-offset.x, 0.f);
	const float3 d = Tex0.SampleLevel(ClampMinMagMipLinear, uvD, srcMipLevel).rgb;
	
	const float3 e = Tex0.SampleLevel(ClampMinMagMipLinear, uvs, srcMipLevel).rgb;
	
	const float2 uvF = uvs + float2(offset.x, 0.f);
	const float3 f = Tex0.SampleLevel(ClampMinMagMipLinear, uvF, srcMipLevel).rgb;
	
	
	const float2 uvG = uvs + float2(-offset.x, offset.y);
	const float3 g = Tex0.SampleLevel(ClampMinMagMipLinear, uvG, srcMipLevel).rgb;
	
	const float2 uvH = uvs + float2(0.f, offset.y);
	const float3 h = Tex0.SampleLevel(ClampMinMagMipLinear, uvH, srcMipLevel).rgb;
	
	const float2 uvI = uvs + float2(offset.x, offset.y);
	const float3 i = Tex0.SampleLevel(ClampMinMagMipLinear, uvI, srcMipLevel).rgb;
	
	float3 color = (a + c + g + i) * 1.f / 16.f;
	color += (b + d + f + h) * 2.f / 16.f;
	color += e * 4.f / 16.f;
	
	// Upsampling levels are in reverse order: <smallest mip> ...3, 2, 1 <largest mip>
	static const uint level = uint(BloomComputeParams.g_bloomRadiusWidthHeightLevelNumLevls.z);
	static const uint numLevels = uint(BloomComputeParams.g_bloomRadiusWidthHeightLevelNumLevls.w);
	if (level == numLevels) // First pass (smallest mip)
	{
		// First pass has no previous (correctly downsampled) mip to sample from
		output0[texelCoord] = float4(color, 0.f);
	}
	else
	{
		// Progressive upscaling: We sum the current blurred result with the previous mip
		const uint prevSrcMip = srcMipLevel + 1;
		const float3 prevBlurredMip = Tex0.SampleLevel(ClampMinMagMipLinear, uvs, prevSrcMip).rgb;
				
		const float avgFactor = 0.5f;
		
		output0[texelCoord] = float4(color + prevBlurredMip, 0.f) * avgFactor;
	}	
}


void BilinearDown(ComputeIn In)
{
	const uint2 texelCoord = In.DTId.xy;
	
	const uint2 dstWidthHeight = uint2(BloomComputeParams.g_dstTexDimensions.xy);
	const uint srcMipLevel = BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.x;
	
	const float2 uvs = PixelCoordsToUV(In.DTId.xy, dstWidthHeight, float2(0.5f, 0.5f));
	
	const float4 color = Tex0.SampleLevel(ClampMinMagMipLinear, uvs, srcMipLevel);
	
	output0[texelCoord] = color;
}


[numthreads(1, 1, 1)]
void CShader(ComputeIn In)
{
	static const bool isDownStage = BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.w > 0.5f;
	
	if (isDownStage)
	{
		static const uint dstMipLevel = BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.y;
		static const uint firstUpsampleSrcMipLevel = BloomComputeParams.g_srcMipDstMipFirstUpsampleSrcMipIsDownStage.z;
		if (dstMipLevel > firstUpsampleSrcMipLevel)
		{
			BilinearDown(In);
		}
		else
		{
			BloomDown(In);
		}		
	}
	else
	{
		BloomUp(In);
	}
}