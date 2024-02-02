// © 2024 Adam Badke. All rights reserved.
#include "CameraCommon.hlsli"
#include "UVUtils.hlsli"

#define XEGTAO_MAIN_PASS
#include "XeGTAOCommon.hlsli"


void ExecuteMainPass(ComputeIn In, const lpfloat sliceCount, const lpfloat stepsPerSlice)
{
	const uint2 pixelCoord = In.DTId.xy;
		
	const uint temporalIdx = 0; // Always 0 without TAA
	const lpfloat2 localNoise = SpatioTemporalNoise(pixelCoord, temporalIdx); // No TAA, so no noise required
	
	const float2 uvs = PixelCoordsToUV(pixelCoord, SEGTAOConstants.ViewportSize, float2(0.5f, 0.5f));
	const float srcMipLevel = 0.f;
	const lpfloat3 worldNormal = (lpfloat3) GBufferWorldNormal.SampleLevel(Clamp_Nearest_Nearest, uvs, srcMipLevel).xyz;
	
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
		Clamp_Nearest_Nearest,
		output0,
		output1);
}