// © 2023 Adam Badke. All rights reserved.
#include "SaberGlobals.hlsli"


struct ComputeIn
{
	uint3 GId	: SV_GroupID;			// Thread group index within the dispatch
	uint3 GTId	: SV_GroupThreadID;		// Local thread ID within the thread group
	uint3 DTId	: SV_DispatchThreadID;	// Global thread ID within the dispatch
	uint GIdx	: SV_GroupIndex;		// Flattened local index within the thread group
};

struct MipGenerationParamsCB
{
	float4 g_textureDimensions; // .xyzw = width, height, 1/width, 1/height
	uint4 g_mipParams; // .x = source mip level, .y = number of mips to output
	bool g_isSRGB;
};
ConstantBuffer<MipGenerationParamsCB> MipGenerationParams;

SamplerState ClampLinearLinear;
Texture2D<float4> SrcTex;

// TODO: If we're using UNORM or SNORM types with UAVs, we need to declare the resource as unorm/snorm
//	-> e.g. RWBuffer<unorm float> uav;
// https://learn.microsoft.com/en-us/windows/win32/direct3d12/typed-unordered-access-view-loads
// -> Build this in to the root sig parser

RWTexture2D<float4> output0 : register(u0);
RWTexture2D<float4> output1 : register(u1);
RWTexture2D<float4> output2 : register(u2);
RWTexture2D<float4> output3 : register(u3);


[numthreads(8, 8, 1)]
void main(ComputeIn In)
{
	// TODO: This is wrong; Implement it correctly
	
	const float2 uvs = PixelCoordsToUV(In.DTId.xy * 2, MipGenerationParams.g_textureDimensions.xy);
	float4 texSample = SrcTex.SampleLevel(ClampLinearLinear, uvs, 0);
	
	if (MipGenerationParams.g_isSRGB)
	{
		//texSample = sRGBToLinear(texSample);
		texSample = LinearToSRGB(texSample);
	}
	
	output0[In.DTId.xy] = texSample;
	output1[In.DTId.xy / 2] = texSample;
	output2[In.DTId.xy / 4] = texSample;
	output3[In.DTId.xy / 8] = texSample;
}