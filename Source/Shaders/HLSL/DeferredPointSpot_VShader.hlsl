// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"

#include "../Common/InstancingParams.h"

#include "../Generated/HLSL/VertexStreams_PositionOnly.hlsli"


StructuredBuffer<TransformData> InstancedTransformParams : register(t1, space1);


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const float4 worldPos = mul(InstancedTransformParams[NonUniformResourceIndex(In.InstanceID)].g_model, float4(In.Position, 1.f));
	
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	return Out;
}