// © 2025 Adam Badke. All rights reserved.
#include "GBufferCommon.hlsli"
#include "SaberComputeCommon.hlsli"
#include "UVUtils.hlsli"

#include "../Common/MaterialParams.h"
#include "../Common/TargetParams.h"

ConstantBuffer<TargetData> TargetParams;

RWTexture2D<float4> LightingTarget : register(u0);


[numthreads(8, 8, 1)]
void CShader(ComputeIn In)
{
	const uint2 targetResolution = TargetParams.g_targetDims.xy;
	const uint2 texelCoord = In.DTId.xy;
	
	if (texelCoord.x >= targetResolution.x || texelCoord.y >= targetResolution.y)
	{
		return;
	}
	
	const GBuffer gbuffer = UnpackGBuffer(texelCoord);
	
	if (gbuffer.MaterialID != MAT_ID_GLTF_Unlit)
	{
		return;
	}

	LightingTarget[texelCoord] = float4(gbuffer.LinearAlbedo, 1.f);
}