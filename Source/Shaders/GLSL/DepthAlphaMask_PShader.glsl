// © 2023 Adam Badke. All rights reserved.
#include "SaberCommon.glsli"


void PShader()
{
	const uint instanceID = InstanceParamsIn.InstanceID;
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[instanceID].g_materialIdx;

	const vec4 matAlbedo = texture(BaseColorTex, In.UV0.xy);

	// Alpha clipping
	const float alphaCutoff = _InstancedPBRMetallicRoughnessParams[materialIdx].g_f0AlphaCutoff.w;
	if (matAlbedo.a < alphaCutoff)
	{
		discard;
	}
}