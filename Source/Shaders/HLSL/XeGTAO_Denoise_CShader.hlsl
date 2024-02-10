// © 2024 Adam Badke. All rights reserved.
#define XEGTAO_DENOISE_PASS
#include "XeGTAOCommon.hlsli"


[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void CShader(ComputeIn In)
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