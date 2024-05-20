// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_INSTANCE_ID
#include "SaberCommon.hlsli"


float4 PShader(VertexOut In) : SV_Target
{
	const uint materialIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_materialIdx;
	
	const float4 matAlbedo = MatAlbedo.Sample(WrapAnisotropic, In.UV0);
	
	// Alpha clipping:
	const float alphaCutoff = InstancedPBRMetallicRoughnessParams[NonUniformResourceIndex(materialIdx)].g_alphaCutoff.x;
	clip(matAlbedo.a < alphaCutoff ? -1 : 1);
	
	return float4(In.Position.z, In.Position.z, In.Position.z, 1.f);
}