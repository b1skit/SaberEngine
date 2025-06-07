// © 2025 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_COLOR
#define SABER_INSTANCING
#include "SaberCommon.hlsli"

#include "GBufferCommon.hlsli"
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


GBufferOut PShader(VertexOut In)
{	
	GBufferOut output;
	
	const uint materialIdx = InstanceIndexParams[In.InstanceID].g_indexes.y;
	
	const float2 albedoUV = GetUV(In, UnlitParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes0.x);
	
	const float4 matAlbedo = BaseColorTex.Sample(WrapAnisotropic, albedoUV);
	
	// Alpha clipping
#if defined(DO_ALPHA_CLIP)
	const float alphaCutoff = UnlitParams[NonUniformResourceIndex(materialIdx)].g_alphaCutuff.x;
	clip(matAlbedo.a < alphaCutoff ? -1 : 1); 
#endif
	
	const float4 baseColorFactor = UnlitParams[NonUniformResourceIndex(materialIdx)].g_baseColorFactor;
	output.Albedo = matAlbedo * baseColorFactor * In.Color;

	output.WorldNormal = float4(0.f, 0.f, 0.f, 0.f);
	
	// RMAOVn:
	const float roughnessFactor = 1.f;	// GLTF Unlit default: Roughness factor > 0.5f
	const float metallicFactor = 0.f;	// GLTF Unlit default: Metallic factor = 0.f
	const float occlusion = 1.f;		// GLTF Unlit default: Occlusion is omitted
	
	output.RMAOVn = float4(roughnessFactor, metallicFactor, occlusion, 0.f);
	
	// Emissive:
	output.Emissive = float4(0.f, 0.f, 0.f, 0.f);	// GLTF Unlit default: Emissive is omitted
	
	// Material properties:
	output.MatProp0Vn = float4(0.f, 0.f, 0.f, 0.f);
	
	// Material ID:
	const uint materialID = UnlitParams[NonUniformResourceIndex(materialIdx)].g_uvChannelIndexes0.y;
	output.MaterialID = materialID;
	
	return output;
}