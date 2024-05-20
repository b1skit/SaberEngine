// © 2023 Adam Badke. All rights reserved.
#define SABER_INSTANCING
#include "SaberCommon.glsl"


void main()
{
	const uint instanceID = InstanceParamsIn.InstanceID;
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[instanceID].g_materialIdx;

	const vec4 matAlbedo = texture(MatAlbedo, In.uv0.xy);

	// Alpha clipping
	const float alphaCutoff = _InstancedPBRMetallicRoughnessParams[materialIdx].g_alphaCutoff.x;
	if (matAlbedo.a < alphaCutoff)
	{
		discard;
	}
}