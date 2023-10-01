// © 2023 Adam Badke. All rights reserved.
#ifndef NORMAL_MAP_UTILS_HLSL
#define NORMAL_MAP_UTILS_HLSL


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
	// Note: In HLSL matrices declared in a shader body do not get packed into constant registers (which is done with 
	// column major ordering). Row-major/column-major packing order has no influence on the packing order of matrix
	// constructors (which always follow row-major ordering)	
	const float3x3 transposeInvRotationScale =
	{
		transposeInvModel[0].xyz,
		transposeInvModel[1].xyz,
		transposeInvModel[2].xyz
	};

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

#endif