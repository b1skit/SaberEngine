// © 2024 Adam Badke. All rights reserved.
#include "CameraCommon.hlsli"
#include "UVUtils.hlsli"

#define XEGTAO_MAIN_PASS
#include "XeGTAOCommon.hlsli"
#include "XeGTAO_MainPass_Common.hlsli"


[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void CShader(ComputeIn In)
{
	// Med quality:
	const lpfloat sliceCount = 2;
	const lpfloat stepsPerSlice = 2;
	
	ExecuteMainPass(In, sliceCount, stepsPerSlice);
}