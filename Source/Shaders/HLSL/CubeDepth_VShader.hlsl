// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"

#include "../Common/InstancingParams.h"

#include "../Generated/HLSL/VertexStreams_PositionUV.hlsli"

// Note: If a resource is used in multiple shader stages, we need to explicitely specify the register and space.
// Otherwise, shader reflection will assign the resource different registers for each stage (while SE expects them to be
// consistent). We (currently) use space1 for all explicit bindings, preventing conflicts with non-explicit bindings in
// space0
StructuredBuffer<InstanceIndexData> InstanceIndexParams : register(t0, space1);
StructuredBuffer<TransformData> TransformParams : register(t1, space1);


VertexOut VShader(VertexIn In)
{
	VertexOut Out;
	
	const uint transformIdx = InstanceIndexParams[In.InstanceID].g_indexes.x;
	const float4 worldPos = mul(TransformParams[NonUniformResourceIndex(transformIdx)].g_model, float4(In.Position, 1.0f));
	
	Out.Position = worldPos;
	
#if defined(VOUT_UV0)
	Out.UV0 = In.UV0;
#endif
#if defined(SABER_INSTANCING)
	Out.InstanceID = In.InstanceID;
#endif
	
	return Out;
}