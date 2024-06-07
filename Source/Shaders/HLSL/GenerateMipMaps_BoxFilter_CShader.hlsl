// © 2023 Adam Badke. All rights reserved.
#include "SaberComputeCommon.hlsli"
#include "Color.hlsli"
#include "UVUtils.hlsli"

#include "../Common/MipGenerationParams.h"


// Based on the technique demonstrated in the Microsoft DirectX Graphics Samples:
// https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/GenerateMipsCS.hlsli

#define SRC_WIDTH_EVEN_HEIGHT_EVEN 0
#define SRC_WIDTH_ODD_HEIGHT_EVEN 1
#define SRC_WIDTH_EVEN_HEIGHT_ODD 2
#define SRC_WIDTH_ODD_HEIGHT_ODD 3

ConstantBuffer<MipGenerationData> MipGenerationParams;

SamplerState ClampMinMagLinearMipPoint;
Texture2DArray<float4> SrcTex;

// It's recommended we use a maximum of 16KB of group shared memory to maximize occupancy on D3D10 hardware, 32KB on
// D3D11 hardware, or unlimited on D3D12 hardware. The actual group shared memory available is hardware-dependant. For
// now, we'll go with the D3D10 minimum, and tune later if/when required.
// For best performance, we want to write to group shared memory addresses in 32-bit offsets relative to a thread's
// group index, using 32-bit types when possible. This will minimize memory controller bank conflicts/collisions.
groupshared float groupSharedChannel_R[64];
groupshared float groupSharedChannel_G[64];
groupshared float groupSharedChannel_B[64];
groupshared float groupSharedChannel_A[64];

void WriteToGroupSharedMemory(uint groupIdx, float4 sample)
{
	groupSharedChannel_R[groupIdx] = sample.r;
	groupSharedChannel_G[groupIdx] = sample.g;
	groupSharedChannel_B[groupIdx] = sample.b;
	groupSharedChannel_A[groupIdx] = sample.a;
}

float4 ReadGroupSharedMemory(uint groupIdx)
{
	return float4(
		groupSharedChannel_R[groupIdx], 
		groupSharedChannel_G[groupIdx], 
		groupSharedChannel_B[groupIdx], 
		groupSharedChannel_A[groupIdx]);
}


// NOTE: Our numthreads choice here is directly related to our group shared memory usage, and thread bitmasking logic
[numthreads(8, 8, 1)]
void CShader(ComputeIn In)
{
	const uint srcMip = MipGenerationParams.g_mipParams.x;
	const uint numMips = MipGenerationParams.g_mipParams.y;
	const uint srcDimensionMode = MipGenerationParams.g_mipParams.z;
	const uint faceIdx = MipGenerationParams.g_mipParams.w;
	
	const float2 output0WidthHeight = MipGenerationParams.g_output0Dimensions.xy;
	const float2 output0TexelWidthHeight = MipGenerationParams.g_output0Dimensions.zw;
	
	const bool isSRGB = MipGenerationParams.g_isSRGB.x > 0.5f;
	
	float4 texSample0 = float4(1.f, 0.f, 1.f, 1.f); // Error: Hot pink
	switch (srcDimensionMode)
	{
		case SRC_WIDTH_EVEN_HEIGHT_EVEN: // 0
		{
			const float2 uvs = PixelCoordsToScreenUV(In.DTId.xy, output0WidthHeight, float2(0.5f, 0.5f));
			texSample0 = SrcTex.SampleLevel(ClampMinMagLinearMipPoint, float3(uvs.xy, faceIdx), srcMip);
		}
		break;
		case SRC_WIDTH_ODD_HEIGHT_EVEN: // 1
		{
			// Width has an odd number of pixels. We blend 2 bilinear samples to prevent undersampling at lower mips
			const float2 leftUVs = PixelCoordsToScreenUV(In.DTId.xy, output0WidthHeight, float2(0.25f, 0.5f));
			const float2 rightUVs = PixelCoordsToScreenUV(In.DTId.xy, output0WidthHeight, float2(0.75f, 0.5f));

			const float4 leftSample = SrcTex.SampleLevel(ClampMinMagLinearMipPoint, float3(leftUVs, faceIdx), srcMip);
			const float4 rightSample = SrcTex.SampleLevel(ClampMinMagLinearMipPoint, float3(rightUVs, faceIdx), srcMip);
			
			texSample0 = (leftSample + rightSample) * 0.5f;
		}
		break;
		case SRC_WIDTH_EVEN_HEIGHT_ODD: // 2
		{
			// Height has an odd number of pixels. We blend 2 bilinear samples to prevent undersampling at lower mips
			const float2 topUVs = PixelCoordsToScreenUV(In.DTId.xy, output0WidthHeight, float2(0.5f, 0.25));
			const float2 botUVs = PixelCoordsToScreenUV(In.DTId.xy, output0WidthHeight, float2(0.5f, 0.75));
			
			const float4 topSample = SrcTex.SampleLevel(ClampMinMagLinearMipPoint, float3(topUVs, faceIdx), srcMip);
			const float4 botSample = SrcTex.SampleLevel(ClampMinMagLinearMipPoint, float3(botUVs, faceIdx), srcMip);
			
			texSample0 = (topSample + botSample) * 0.5f;
		}
		break;
		case SRC_WIDTH_ODD_HEIGHT_ODD: // 3
		{
			// Both the width and height are odd. We blend 4 samples to prevent undersampling at lower mips
			const float2 topLeftUVs = PixelCoordsToScreenUV(In.DTId.xy, output0WidthHeight, float2(0.25f, 0.25f));
			const float2 topRightUVs = PixelCoordsToScreenUV(In.DTId.xy, output0WidthHeight, float2(0.75f, 0.25f));
			const float2 botLeftUVs = PixelCoordsToScreenUV(In.DTId.xy, output0WidthHeight, float2(0.25f, 0.75f));
			const float2 botRightUVs = PixelCoordsToScreenUV(In.DTId.xy, output0WidthHeight, float2(0.75f, 0.75f));
			
			const float4 topLeftSample = SrcTex.SampleLevel(ClampMinMagLinearMipPoint, float3(topLeftUVs, faceIdx), srcMip);
			const float4 topRightSample = SrcTex.SampleLevel(ClampMinMagLinearMipPoint, float3(topRightUVs, faceIdx), srcMip);
			const float4 botLeftSample = SrcTex.SampleLevel(ClampMinMagLinearMipPoint, float3(botLeftUVs, faceIdx), srcMip);
			const float4 botRightSample = SrcTex.SampleLevel(ClampMinMagLinearMipPoint, float3(botRightUVs, faceIdx), srcMip);
			
			texSample0 = (topLeftSample + topRightSample + botLeftSample + botRightSample) * 0.25f;
		}
		break;
	}
	
	output0[In.DTId.xy] = isSRGB ? LinearToSRGB(texSample0) : texSample0;
	
	if (numMips == 1)
	{
		return;
	}

	// Share our local sample via group shared memory
	WriteToGroupSharedMemory(In.GIdx, texSample0);
	
	GroupMemoryBarrierWithGroupSync();
	
	// For the next mip level, we only need 1/4 of our threads to contribute. We select the top-left pixel out of every
	// 2x2 block in the current 8x8 threadgroup, which will satisfy (x % 2 == 0 && y % 2 == 0). We can avoid the costly
	// modulo operator by bitmasking our flatted thread group index with 0x9 == 0b00001001, giving us threads
	// 0, 2, 4, 6, 16, 18, 20, 22, 32, 34, 36, 38, 48, 50, 52, 54... Note: This is contingent on numthreads = [8,8,1]
	if ((In.GIdx & 0x9) == 0)
	{
		// We want to wrap our sample indexes such that we retrieve the results of the thread immediately to the right,
		// below, and bottom-right of this thread. Note: These offsets are dependent on our 8x8 thread group dimensions
		const float4 rightSample = ReadGroupSharedMemory(In.GIdx + 1);
		const float4 bottomSample = ReadGroupSharedMemory(In.GIdx + 8);
		const float4 bottomRightSample = ReadGroupSharedMemory(In.GIdx + 9);
		
		texSample0 += rightSample + bottomSample + bottomRightSample;
		texSample0 *= 0.25f;
		
		const uint2 output1Coords = In.DTId.xy / 2;
		output1[output1Coords] = isSRGB ? LinearToSRGB(texSample0) : texSample0;
		
		WriteToGroupSharedMemory(In.GIdx, texSample0);
	}
	
	if (numMips == 2)
	{
		return;
	}
	GroupMemoryBarrierWithGroupSync();
	
	// We only need 1/8 of our threads to contribute: Select threads with X and Y coordinates that are a multiple of 8.
	// The logical AND of our flattended group index with 0x1B == 0b00011011 gives us 0, 4, 32, 36 (assuming 
	// numthreads = [8,8,1])
	if ((In.GIdx & 0x1B) == 0)
	{
		// Wrap our sample indexes to the index of the top-left pixel of the 2x2 block neighboring the current thread to
		// the right, below, and bottom-right
		const float4 rightSample = ReadGroupSharedMemory(In.GIdx + 2);
		const float4 bottomSample = ReadGroupSharedMemory(In.GIdx + 16);
		const float4 bottomRightSample = ReadGroupSharedMemory(In.GIdx + 18);
		
		texSample0 += rightSample + bottomSample + bottomRightSample;
		texSample0 *= 0.25f;

		const uint2 output2Coords = In.DTId.xy / 4;
		output2[output2Coords] = isSRGB ? LinearToSRGB(texSample0) : texSample0;
		
		WriteToGroupSharedMemory(In.GIdx, texSample0);
	}
	
	if (numMips == 3)
	{
		return;
	}
	GroupMemoryBarrierWithGroupSync();

	// Finally, we only need a single thread to combine the last 2x2 block of samples:
	if (In.GIdx == 0)
	{
		const float4 rightSample = ReadGroupSharedMemory(In.GIdx + 4);
		const float4 bottomSample = ReadGroupSharedMemory(In.GIdx + 32);
		const float4 bottomRightSample = ReadGroupSharedMemory(In.GIdx + 36);
		
		texSample0 += rightSample + bottomSample + bottomRightSample;
		texSample0 *= 0.25f;
		
		const uint2 output3Coords = In.DTId.xy / 8;
		output3[output3Coords] = isSRGB ? LinearToSRGB(texSample0) : texSample0;
	}	
}