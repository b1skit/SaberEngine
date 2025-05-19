// © 2025 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_COLOR
#define SABER_INSTANCING
#include "SaberCommon.hlsli"

#include "UVUtils.hlsli"

#include "../Common/InstancingParams.h"
#include "../Common/MaterialParams.h"

Texture2D<float4> BaseColorTex;

// Note: If a resource is used in multiple shader stages, we need to explicitely specify the register and space.
// Otherwise, shader reflection will assign the resource different registers for each stage (while SE expects them to be
// consistent). We (currently) use space1 for all explicit bindings, preventing conflicts with non-explicit bindings in
// space0
StructuredBuffer<InstanceIndexData> InstanceIndexParams : register(t0, space1);
StructuredBuffer<UnlitData> UnlitParams : register(t2, space1);


float4 PShader(VertexOut In) : SV_Target
{
	const uint materialIdx = InstanceIndexParams[In.InstanceID].g_indexes.y;
	
	const float2 albedoUV = GetUV(In, UnlitParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes0.x);
	
	const float4 matAlbedo = BaseColorTex.Sample(WrapAnisotropic, albedoUV);
	const float4 baseColorFactor = UnlitParams[NonUniformResourceIndex(materialIdx)].g_baseColorFactor;
	const float4 albedoAlpha = matAlbedo * In.Color * baseColorFactor;
	
	return albedoAlpha;
}