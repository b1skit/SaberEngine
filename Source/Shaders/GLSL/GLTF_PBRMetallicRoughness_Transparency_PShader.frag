// © 2023 Adam Badke. All rights reserved.
#define SABER_VEC4_OUTPUT
#define VOUT_COLOR
#define VOUT_TBN
#define SABER_INSTANCING

#include "NormalMapUtils.glsl"
#include "SaberCommon.glsl"


void main()
{
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[InstanceParamsIn.InstanceID].g_materialIdx;

	const vec4 matAlbedo = texture(MatAlbedo, In.uv0.xy);

	// TODO: Apply lighting. For now, just return the albedo
	FragColor = matAlbedo;
}