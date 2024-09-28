// © 2023 Adam Badke. All rights reserved.
#define VOUT_TBN
#define VOUT_COLOR
#define SABER_INSTANCING
#include "NormalMapUtils.glsli"
#include "SaberCommon.glsli"

#include "../Generated/GLSL/VertexStreams_PosNmlTanUvCol.glsli"


#if defined(HAS_MORPH_TARGETS)

layout(std430, binding=16) readonly buffer MorphData { vec4 _MorphData[]; };

vec3 UnpackVec3(
	uint vertexIndex, // For the current vertex
	uint firstFloatIdx, // Within the buffer of interleaved morph target data
	uint vertexStride, // No. of floats for all targets for a single vertex
	uint elementStride, // No. of floats in the element type we're retrieving
	uint morphTargetIdx) // Index of the morph target for the current vertex attribute
{
	vec3 result = vec3(0.f, 0.f, 0.f);
	
	for (uint i = 0; i < 3; ++i)
	{
		const uint baseIdx = firstFloatIdx + (vertexIndex * vertexStride) + (morphTargetIdx * elementStride) + i;
		
		const uint idxFloat4 = baseIdx / 4;
		const uint idxComponent = baseIdx % 4;

		result[i] = _MorphData[idxFloat4][idxComponent];
	}
	return result;
}

#endif


void VShader()
{
	const uint transformIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_transformIdx;
	const uint materialIdx = _InstanceIndexParams.g_instanceIndices[gl_InstanceID].g_materialIdx;

	vec3 position = Position;
	
	// TODO: Implement this correctly. For now, just prove we're getting the data we need
#if defined(HAS_MORPH_TARGETS)
	const uint firstFloatIdx = 0;
	const uint vertexStride = 6;
	const uint elementStride = 3;
	const uint morphTargetIdx = 0;

	position += UnpackVec3(gl_VertexID, firstFloatIdx, vertexStride, elementStride, morphTargetIdx);
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