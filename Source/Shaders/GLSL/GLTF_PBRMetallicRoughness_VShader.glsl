// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN
#define VOUT_COLOR
#define SABER_INSTANCING
#include "NormalMapUtils.glsli"
#include "SaberCommon.glsli"

#if defined(MORPH_POS8)
#include "../Generated/GLSL/VertexStreams_PosNmlTanUvCol_Morph_Pos8.glsli"
#else
#include "../Generated/GLSL/VertexStreams_PosNmlTanUvCol.glsli"
#endif



void VShader()
{
	const uint transformIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_transformIdx;
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_materialIdx;

	vec3 position = Position;
	
	// TODO: Implement this correctly. For now, just prove we're getting the data we need
#if defined(MORPH_POS8)
	position += PositionMorph1;
#endif

	const vec4 worldPos = _InstancedTransformParams[transformIdx].g_model * vec4(position, 1.0f);
	gl_Position = _CameraParams.g_viewProjection * worldPos;
	
#if defined(VOUT_WORLD_POS)
	Out.WorldPos = worldPos.xyz;
#endif

	Out.UV0 = UV0;

#if MAX_UV_CHANNEL_IDX >= 1
	Out.UV1 = UV1;
#endif

	Out.Color = _InstancedPBRMetallicRoughnessParams[materialIdx].g_baseColorFactor * Color;

	Out.TBN = BuildTBN(Normal, Tangent, _InstancedTransformParams[transformIdx].g_transposeInvModel);
	
	InstanceParamsOut.InstanceID = gl_InstanceID;
}