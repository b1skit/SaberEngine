// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.hlsli"

#include "../Common/InstancingParams.h"
#include "../Common/MaterialParams.h"


Texture2D<float4> BaseColorTex;

// If a resource is used in multiple shader stages, we need to explicitely specify the register and space. Otherwise,
// shader reflection will assign the resource different registers for each stage (while SE expects them to be consistent).
// We (currently) use space1 for all explicit bindings, preventing conflicts with non-explicit bindings in space0
ConstantBuffer<InstanceIndexData> InstanceIndexParams : register(b0, space1);

StructuredBuffer<PBRMetallicRoughnessData> InstancedPBRMetallicRoughnessParams : register(t1, space1);


float4 PShader(VertexOut In) : SV_Target
{
	const uint materialIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_materialIdx;
	
	const float4 matAlbedo = BaseColorTex.Sample(WrapAnisotropic, In.UV0);
	
	// Alpha clipping:
	const float alphaCutoff = InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_f0AlphaCutoff.w;
	clip(matAlbedo.a < alphaCutoff ? -1 : 1);
	
	return float4(In.Position.z, In.Position.z, In.Position.z, 1.f);
}