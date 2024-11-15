// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"

#include "../Common/InstancingParams.h"

#include "../Generated/HLSL/VertexStreams_PositionUV.hlsli"

// If a resource is used in multiple shader stages, we need to explicitely specify the register and space. Otherwise,
// shader reflection will assign the resource different registers for each stage (while SE expects them to be consistent).
// We (currently) use space1 for all explicit bindings, preventing conflicts with non-explicit bindings in space0
ConstantBuffer<InstanceIndexData> InstanceIndexParams : register(b0, space1);

StructuredBuffer<InstancedTransformData> InstancedTransformParams : register(t0, space1); // Indexed by instance ID


VertexOut VShader(VertexIn In)
{
	VertexOut Out;
	
	const uint transformIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_transformIdx;

	const float4 worldPos = mul(InstancedTransformParams[transformIdx].g_model, float4(In.Position, 1.0f));
	Out.Position = worldPos;
	
#if defined(VOUT_UV0)
	Out.UV0 = In.UV0;
#endif
#if defined(SABER_INSTANCING)
	Out.InstanceID = In.InstanceID;
#endif
	
	return Out;
}