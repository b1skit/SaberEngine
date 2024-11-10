// © 2024 Adam Badke. All rights reserved.
#version 460 // Suppress IDE warnings; Stripped out at compile time
#include "NormalMapUtils.glsli"

#include "../Common/AnimationParams.h"


layout(std430, binding=0) readonly buffer InPosition { float data[]; } _InPosition;
layout(std430, binding=1) readonly buffer InNormal { float data[]; } _InNormal;
layout(std430, binding=2) readonly buffer InTangent { vec4 data[]; } _InTangent;

layout(std430, binding=3) readonly buffer InBlendIndices { float data[]; } _InBlendIndices;
layout(std430, binding=4) readonly buffer InBlendWeights { float data[]; } _InBlendWeights;

layout(std430, binding=5) readonly buffer SkinningJoints { mat4 data[]; } _SkinningJoints;
layout(std430, binding=6) readonly buffer TransposeInvSkinningJoints { mat4 data[]; } _TransposeInvSkinningJoints;

layout(std430, binding=7) buffer OutPosition { float data[]; } _OutPosition;
layout(std430, binding=8) buffer OutNormal { float data[]; } _OutNormal;
layout(std430, binding=9) buffer OutTangent { vec4 data[]; } _OutTangent;

layout(binding=9) uniform SkinningParams { SkinningData _SkinningParams; };


// OpenGL's std430 layout aligns vec3 on vec4/16B boundaries, so we declare our buffer data as arrays of floats and
// unpack/pack them
vec3 GetInPosition(uint vertexIndex)
{
	const uint baseIdx = vertexIndex * 3;
	return vec3(
		_InPosition.data[baseIdx],
		_InPosition.data[baseIdx + 1],
		_InPosition.data[baseIdx + 2]);
}


void SetOutPosition(uint vertexIndex, vec3 outPos)
{
	const uint baseIdx = vertexIndex * 3;
	_OutPosition.data[baseIdx] = outPos.x;
	_OutPosition.data[baseIdx + 1] = outPos.y;
	_OutPosition.data[baseIdx + 2] = outPos.z;
}


vec3 GetInNormal(uint vertexIndex)
{
	const uint baseIdx = vertexIndex * 3;
	return vec3(
		_InNormal.data[baseIdx],
		_InNormal.data[baseIdx + 1],
		_InNormal.data[baseIdx + 2]);
}


void SetOutNormal(uint vertexIndex, vec3 outNml)
{
	const uint baseIdx = vertexIndex * 3;
	_OutNormal.data[baseIdx] = outNml.x;
	_OutNormal.data[baseIdx + 1] = outNml.y;
	_OutNormal.data[baseIdx + 2] = outNml.z;
}


layout (local_size_x = VERTEX_ANIM_THREADS_X, local_size_y = 1, local_size_z = 1) in;
void CShader()
{
	const uint numVertsPerStream = _SkinningParams.g_meshPrimMetadata.x;
	
	// Early out if our thread index doesn't correspond to a valid vertex index
	const uint vertexIndex = gl_GlobalInvocationID.x;
	if (vertexIndex >= numVertsPerStream)
	{
		return;
	}	
	
	const uint maxInfluences = 4; // TODO: Support multiple sets (i.e. > 4 influences per joint)
	
	const vec3 inPos = GetInPosition(vertexIndex);
	const vec3 inNml = GetInNormal(vertexIndex).xyz;
	const vec4 inTangent = _InTangent.data[vertexIndex];

	vec4 outPos = vec4(0.f, 0.f, 0.f, 0.f);
	vec3 outNml = vec3(0.f, 0.f, 0.f);
	vec4 outTangent = vec4(0.f, 0.f, 0.f, 0.f);
	
	for (uint i = 0; i < 4; ++i)
	{
		const uint blendSrcIndex = (vertexIndex * maxInfluences) + i;

		const float jointWeight = _InBlendWeights.data[blendSrcIndex];
		if (jointWeight > 0)
		{
			const uint jointIdx = uint(_InBlendIndices.data[blendSrcIndex]);

			// Position:
			outPos += _SkinningJoints.data[jointIdx] * vec4(inPos, 1.f) * jointWeight;
			
			// Normal:
			const mat3 transposeInvRotationScale = GetTransposeInvRotationScale(_TransposeInvSkinningJoints.data[jointIdx]);
			
			outNml += (transposeInvRotationScale * inNml) * jointWeight;
			
			// Tangent:
			outTangent.xyz += (transposeInvRotationScale * inTangent.xyz) * jointWeight;
			outTangent.w = inTangent.w; // Sign bit is packed into .w == 1.0 or -1.0			
		}
	}
	
	SetOutPosition(vertexIndex, outPos.xyz);
	SetOutNormal(vertexIndex, normalize(outNml));
	_OutTangent.data[vertexIndex] = normalize(outTangent);
}