// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"

#include "../Common/InstancingParams.h"

#include "../Generated/HLSL/VertexStreams_PositionOnly.hlsli"


// If a resource is used in multiple shader stages, we need to explicitely specify the register and space. Otherwise,
// shader reflection will assign the resource different registers for each stage (while SE expects them to be consistent).
// We (currently) use space1 for all explicit bindings, preventing conflicts with non-explicit bindings in space0
StructuredBuffer<TransformData> InstancedTransformParams : register(t0, space1); // Indexed by instance ID


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const float4 worldPos = mul(InstancedTransformParams[In.InstanceID].g_model, float4(In.Position, 1.f));
	
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);
	
	return Out;
}