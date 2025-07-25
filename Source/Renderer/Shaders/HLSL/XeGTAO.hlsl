// © 2024 Adam Badke. All rights reserved.
#include "UVUtils.hlsli"
#include "XeGTAOCommon.hlsli"

#include "../Common/CameraParams.h"

ConstantBuffer<CameraData> CameraParams : register(space1);


void ExecuteMainPass(ComputeIn In, const lpfloat sliceCount, const lpfloat stepsPerSlice)
{
	const uint2 pixelCoord = In.DTId.xy;
		
	const uint temporalIdx = 0; // Always 0 without TAA
	const lpfloat2 localNoise = SpatioTemporalNoise(pixelCoord, temporalIdx); // No TAA, so no noise required
	
	const float2 uvs = PixelCoordsToScreenUV(pixelCoord, SEGTAOConstants.ViewportSize, float2(0.5f, 0.5f));
	const float srcMipLevel = 0.f;
	const lpfloat3 worldNormal = (lpfloat3) SceneWNormal.SampleLevel(ClampMinMagMipPoint, uvs, srcMipLevel).xyz;
	
	// Convert our normals to view space:
	lpfloat3 viewNormal = (lpfloat3) mul((float3x3) CameraParams.g_view, worldNormal);
	viewNormal.z *= (lpfloat) (-1.0); // Flip Z: SaberEngine uses a RHCS
	
	XeGTAO_MainPass(
		pixelCoord,
		sliceCount,
		stepsPerSlice,
		localNoise,
		viewNormal,
		SEGTAOConstants,
		PrefilteredDepth,
		ClampMinMagMipPoint,
		output0,
		output1);
}


[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void MainPass_Low(ComputeIn In)
{
	// Low quality:
	const lpfloat sliceCount = 1;
	const lpfloat stepsPerSlice = 2;
	
	ExecuteMainPass(In, sliceCount, stepsPerSlice);
}


[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void MainPass_Med(ComputeIn In)
{
	// Med quality:
	const lpfloat sliceCount = 2;
	const lpfloat stepsPerSlice = 2;
	
	ExecuteMainPass(In, sliceCount, stepsPerSlice);
}


[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void MainPass_High(ComputeIn In)
{
	// High quality:
	const lpfloat sliceCount = 3;
	const lpfloat stepsPerSlice = 3;
	
	ExecuteMainPass(In, sliceCount, stepsPerSlice);
}


[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void MainPass_Ultra(ComputeIn In)
{
	// Ultra quality:
	const lpfloat sliceCount = 9;
	const lpfloat stepsPerSlice = 3;
	
	ExecuteMainPass(In, sliceCount, stepsPerSlice);
}


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


[numthreads(8, 8, 1)]
void CShader(ComputeIn In)
{
	XeGTAO_PrefilterDepths16x16(
		In.DTId.xy /*: SV_DispatchThreadID*/,
		In.GTId.xy /*: SV_GroupThreadID*/,
		SEGTAOConstants,
		SceneDepth,
		ClampMinMagMipPoint,
		output0,
		output1,
		output2,
		output3,
		output4);
}