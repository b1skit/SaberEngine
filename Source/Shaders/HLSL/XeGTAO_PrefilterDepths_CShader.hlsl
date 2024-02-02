// © 2024 Adam Badke. All rights reserved.
#include "XeGTAOCommon.hlsli"


RWTexture2D<lpfloat> output0 : register(u0);
RWTexture2D<lpfloat> output1 : register(u1);
RWTexture2D<lpfloat> output2 : register(u2);
RWTexture2D<lpfloat> output3 : register(u3);
RWTexture2D<lpfloat> output4 : register(u4);

Texture2D<float> Depth0;


[numthreads(8, 8, 1)]
void CShader(ComputeIn In)
{
	XeGTAO_PrefilterDepths16x16(
		In.DTId.xy /*: SV_DispatchThreadID*/,
		In.GTId.xy /*: SV_GroupThreadID*/,
		SEGTAOConstants,
		Depth0,
		Clamp_Nearest_Nearest,
		output0,
		output1,
		output2,
		output3,
		output4);
}