// © 2024 Adam Badke. All rights reserved.
#include "XeGTAOCommon.hlsli"


[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void Denoise(ComputeIn In)
{
	// We compute 2 horizontal pixels at a time as a performance optimization:
	const uint2 pixelCoordBase = In.DTId.xy * uint2(2, 1);
	
	const bool isLastPass = false;
	
	XeGTAO_Denoise(
		pixelCoordBase,
		SEGTAOConstants,
		SourceAO, // Working AO term
		SourceEdges, // Working edges
		ClampMinMagMipPoint,
		output0,
		isLastPass);
}


[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void Denoise_LastPass(ComputeIn In)
{
	// We compute 2 horizontal pixels at a time as a performance optimization:
	const uint2 pixelCoordBase = In.DTId.xy * uint2(2, 1);
	
	if (SEXeGTAOSettings.g_enabled == 0.f)
	{
		output0[pixelCoordBase] = 255; // Pure white
		output0[pixelCoordBase + uint2(1, 0)] = 255;
		return;
	}
	
	const bool isLastPass = true;
	
	XeGTAO_Denoise(
		pixelCoordBase,
		SEGTAOConstants,
		SourceAO, // Working AO term
		SourceEdges, // Working edges
		ClampMinMagMipPoint,
		output0,
		isLastPass);
}