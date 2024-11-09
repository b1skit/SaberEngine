// © 2024 Adam Badke. All rights reserved.
#define SPECIFY_OWN_COMPUTE_OUTPUTS
#include "SaberComputeCommon.hlsli"
#include "NormalMapUtils.hlsli"

#include "../Common/AnimationParams.h"


StructuredBuffer<float3> InPosition;
StructuredBuffer<float3> InNormal;
StructuredBuffer<float4> InTangent;

StructuredBuffer<float> InBlendIndices;
StructuredBuffer<float> InBlendWeights;

StructuredBuffer<float4x4> SkinningJoints;
StructuredBuffer<float4x4> TransposeInvSkinningJoints;

RWStructuredBuffer<float3> OutPosition;
RWStructuredBuffer<float3> OutNormal;
RWStructuredBuffer<float4> OutTangent;

ConstantBuffer<SkinningData> SkinningParams;


[numthreads(VERTEX_ANIM_THREADS_X, 1, 1)]
void CShader(ComputeIn In)
{
	const uint numVertsPerStream = SkinningParams.g_meshPrimMetadata.x;
	
	// Early out if our thread index doesn't correspond to a valid vertex index
	const uint vertexIndex = In.DTId.x;
	if (vertexIndex >= numVertsPerStream)
	{
		return;
	}	
	
	const uint maxInfluences = 4; // TODO: Support multiple sets (i.e. > 4 influences per joint)
	
	const float4 inPos = float4(InPosition[vertexIndex], 1.f);
	const float3 inNml = InNormal[vertexIndex];
	const float4 inTangent = InTangent[vertexIndex];
	
	float4 outPos = float4(0.f, 0.f, 0.f, 0.f);
	float3 outNml = float3(0.f, 0.f, 0.f);
	float4 outTangent = float4(0.f, 0.f, 0.f, 0.f);
		
	[unroll(4)]
	for (uint i = 0; i < 4; ++i)
	{
		const uint blendSrcIndex = (vertexIndex * maxInfluences) + i;
		
		const float jointWeight = InBlendWeights[blendSrcIndex];
		if (jointWeight > 0)
		{
			const uint jointIdx = InBlendIndices[blendSrcIndex];
		
			// Position:
			outPos += mul(SkinningJoints[jointIdx], inPos) * jointWeight;
			
			// Normal:
			const float3x3 transposeInvRotationScale = GetTransposeInvRotationScale(TransposeInvSkinningJoints[jointIdx]);
			
			outNml += mul(transposeInvRotationScale, inNml) * jointWeight;
			
			// Tangent:
			outTangent.xyz += mul(transposeInvRotationScale, inTangent.xyz) * jointWeight;
			outTangent.w = inTangent.w; // Sign bit is packed into .w == 1.0 or -1.0
		}
	}
	
	OutPosition[vertexIndex] = outPos.xyz;
	OutNormal[vertexIndex] = outNml;
	OutTangent[vertexIndex] = outTangent;
}