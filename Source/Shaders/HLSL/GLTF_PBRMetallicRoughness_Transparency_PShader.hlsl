// © 2023 Adam Badke. All rights reserved.
#define VOUT_UV0
#define VOUT_TBN
#define VOUT_COLOR
#define VOUT_INSTANCE_ID

#include "NormalMapUtils.hlsli"
#include "SaberCommon.hlsli"


float4 PShader(VertexOut In) : SV_Target
{
	const uint materialIdx = InstanceIndexParams.g_instanceIndices[In.InstanceID].g_materialIdx;
	
	const float4 matAlbedo = MatAlbedo.Sample(WrapAnisotropic, In.UV0);
	
	// TODO: Apply lighting. For now, just return the albedo
	return matAlbedo;
}