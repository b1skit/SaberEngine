// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_TBN
#define VOUT_COLOR
#define SABER_INSTANCING
#include "NormalMapUtils.hlsli"
#include "SaberCommon.hlsli"

#include "../Common/InstancingParams.h"
#include "../Common/MaterialParams.h"

#include "../Generated/HLSL/VertexStreams_PosNmlTanUvCol.hlsli"


// Note: If a resource is used in multiple shader stages, we need to explicitely specify the register and space.
// Otherwise, shader reflection will assign the resource different registers for each stage (while SE expects them to be
// consistent). We (currently) use space1 for all explicit bindings, preventing conflicts with non-explicit bindings in
// space0
StructuredBuffer<InstanceIndexData> InstanceIndexParams : register(t0, space1);
StructuredBuffer<TransformData> InstancedTransformParams : register(t1, space1);
StructuredBuffer<PBRMetallicRoughnessData> InstancedPBRMetallicRoughnessParams : register(t2, space1);


VertexOut VShader(VertexIn In)
{
	VertexOut Out;

	const uint transformIdx = InstanceIndexParams[In.InstanceID].g_transformIdx;
	const uint materialIdx = InstanceIndexParams[In.InstanceID].g_materialIdx;
	
	float3 position = In.Position;
	
	const float4 worldPos = mul(InstancedTransformParams[transformIdx].g_model, float4(position, 1.0f));
	Out.Position = mul(CameraParams.g_viewProjection, worldPos);

#if defined(VOUT_WORLD_POS)
	Out.WorldPos = worldPos.xyz;
#endif
	
	Out.UV0 = In.UV0;
	
#if MAX_UV_CHANNEL_IDX >= 1
	Out.UV1 = In.UV1;
#endif

	Out.Color = InstancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor * In.Color;
	
	Out.TBN = BuildTBN(In.Normal, In.Tangent, InstancedTransformParams[transformIdx].g_transposeInvModel);
	
	Out.InstanceID = In.InstanceID;
	
	return Out;	
}