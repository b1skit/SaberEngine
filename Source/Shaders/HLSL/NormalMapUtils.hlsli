// © 2023 Adam Badke. All rights reserved.
#ifndef NORMAL_MAP_UTILS_HLSL
#define NORMAL_MAP_UTILS_HLSL

#include "Transformations.hlsli"


float3 WorldNormalFromTextureNormal(float3 texNormal, float3 normalScale, float3x3 TBN)
{
	texNormal = normalize((texNormal * 2.f) - 1.f); // [0, 1] -> [-1, 1]
	texNormal *= normalScale;
	
	return normalize(mul(TBN, texNormal));
}


// When rotating normal vectors we use the transpose of the inverse of the model matrix, incase we have a
// non-uniform scaling factor
// https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/geometry/transforming-normals
// This effectively isolates the inverse of the scale component (as the inverse and transpose of a rotation matrix
// cancel each other)
float3x3 BuildTBN(float3 inFaceNormal, float4 inLocalTangent, float4x4 transposeInvModel)
{
	const float3x3 transposeInvRotationScale = GetTransposeInvRotationScale(transposeInvModel);

	const float signBit = inLocalTangent.w; // Sign bit is packed into localTangent.w == 1.0 or -1.0

	const float3 worldFaceNormal = normalize(mul(transposeInvRotationScale, inFaceNormal));
	float3 worldTangent = normalize(mul(transposeInvRotationScale, inLocalTangent.xyz));
	
	// Apply Gram-Schmidt re-orthogonalization to the Tangent:
	worldTangent = normalize(worldTangent - (dot(worldTangent, worldFaceNormal) * worldFaceNormal));

	const float3 worldBitangent = normalize(cross(worldFaceNormal.xyz, worldTangent.xyz) * signBit);
	
	// Matrix ctors pack in row-major ordering: Insert our TBN vectors as columns
	return float3x3(
		worldTangent.x, worldBitangent.x, worldFaceNormal.x,
		worldTangent.y, worldBitangent.y, worldFaceNormal.y,
		worldTangent.z, worldBitangent.z, worldFaceNormal.z);
}


// Octohedral normal map packing/unpacking as per Krzysztof Narkowicz
// https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/
float2 OctWrap(float2 v)
{
	return (1.f - abs(v.yx)) * (v.xy >= 0.f ? 1.f : -1.f);
}


float2 EncodeOctohedralNormal(float3 n)
{
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	n.xy = n.z >= 0.f ? n.xy : OctWrap(n.xy);
	n.xy = n.xy * 0.5 + 0.5;
	return n.xy;
}
 

float3 DecodeOctohedralNormal(float2 f)
{
	f = f * 2.f - 1.f;
 
	// https://twitter.com/Stubbesaurus/status/937994790553227264
	float3 n = float3(f.x, f.y, 1.f - abs(f.x) - abs(f.y));
	float t = saturate(-n.z);
	n.xy += n.xy >= 0.f ? -t : t;
	return normalize(n);
}


#endif