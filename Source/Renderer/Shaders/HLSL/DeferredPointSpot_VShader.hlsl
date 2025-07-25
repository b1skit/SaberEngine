// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"

#include "../Common/CameraParams.h"
#include "../Common/InstancingParams.h"
#include "../Common/TransformParams.h"

#include "../_generated/HLSL/VertexStreams_PositionOnly.hlsli"

ConstantBuffer<CameraData> CameraParams : register(space1);

StructuredBuffer<InstanceIndexData> InstanceIndexParams : register(t0, space1);
StructuredBuffer<TransformData> TransformParams : register(t1, space1);


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const uint transformIdx = InstanceIndexParams[NonUniformResourceIndex(In.InstanceID)].g_indexes.x;
	
	const float4 worldPos = mul(TransformParams[NonUniformResourceIndex(transformIdx)].g_model, float4(In.Position, 1.f));
	
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	return Out;
}