// © 2024 Adam Badke. All rights reserved.
#ifndef NORMAL_MAP_UTILS_HLSL
#define NORMAL_MAP_UTILS_HLSL


vec3 WorldNormalFromTextureNormal(vec3 texNormal, mat3 TBN)
{
	const vec3 normal = (texNormal * 2.f) - 1.f; // Transform [0,1] -> [-1,1]

	return normalize(TBN * normal);
}


// When rotating normal vectors we use the transpose of the inverse of the model matrix, incase we have a
// non-uniform scaling factor
// https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/geometry/transforming-normals
// This effectively isolates the inverse of the scale component (as the inverse and transpose of a rotation matrix
// cancel each other)
mat3 BuildTBN(const vec3 inFaceNormal, const vec4 inLocalTangent, const mat4 transposeInvModel)
{
	const mat3 transposeInvRotationScale = mat3(transposeInvModel);

	const float signBit = inLocalTangent.w; // Sign bit is packed into localTangent.w == 1.0 or -1.0

	const vec3 worldFaceNormal = normalize(transposeInvRotationScale * inFaceNormal);
	vec3 worldTangent = normalize(transposeInvRotationScale * inLocalTangent.xyz);
	
	// Apply Gram-Schmidt re-orthogonalization to the Tangent:
	worldTangent = normalize(worldTangent - (dot(worldTangent, worldFaceNormal) * worldFaceNormal));

	const vec3 worldBitangent = normalize(cross(worldFaceNormal.xyz, worldTangent.xyz) * signBit);
	
	// Note: In GLSL, matrix components are constructed/consumed in column major order
	return mat3(worldTangent, worldBitangent, worldFaceNormal);
}

#endif