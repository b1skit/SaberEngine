#ifndef SABER_GLOBALS
#define SABER_GLOBALS

#include "MathConstants.glsl"


#if defined(READ_GBUFFER)
struct GBuffer
{
	vec3 LinearAlbedo;
	vec3 WorldNormal;

	float LinearRoughness;
	float LinearMetalness;
	float AO;

#if defined(GBUFFER_EMISSIVE)
	vec3 Emissive;
#endif
	vec3 MatProp0; // .rgb = F0 (Surface response at 0 degrees)
	float NonLinearDepth;
};


GBuffer UnpackGBuffer(vec2 screenUV)
{
	GBuffer gbuffer;

	// Note: All PBR calculations are performed in linear space
	// However, we use sRGB-format input textures, the sRGB->Linear transformation happens for free when writing the 
	// GBuffer, so no need to do the sRGB -> linear conversion here
	gbuffer.LinearAlbedo = texture(GBufferAlbedo, screenUV).rgb;

	gbuffer.WorldNormal = texture(GBufferWNormal, screenUV).xyz;

	const vec3 RMAO = texture(GBufferRMAO, screenUV).rgb;
	gbuffer.LinearRoughness = RMAO.r;
	gbuffer.LinearMetalness = RMAO.g;
	gbuffer.AO = RMAO.b;

#if defined(GBUFFER_EMISSIVE)
	gbuffer.Emissive = texture(GBufferEmissive, screenUV).rgb;
#endif

	gbuffer.MatProp0 = texture(GBufferMatProp0, screenUV).rgb;

	gbuffer.NonLinearDepth = texture(GBufferDepth, screenUV).r;

	return gbuffer;
}
#endif


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


vec3 WorldNormalFromTextureNormal(vec3 texNormal, mat3 TBN)
{
	const vec3 normal = (texNormal * 2.f) - 1.f; // Transform [0,1] -> [-1,1]

	return normalize(TBN * normal);
}


/* 
* Convert a world-space direction to spherical (latitude-longitude) map [0,1] UVs.
* The lat/long map is centered about the -Z axis, wrapping clockwise (left to right)
* Note:
* atan(y, x) is the angle between the positive x axis of a plane, and the point (x, y) on it
* - y is the numerator, x is the denominator ->  atan(y/x)
* - The angle is in [-pi, pi]
* - Result is Postive when y > 0 (i.e. the point is above the x axis), and negative when y < 0 (lower half plane)
* 	
* Thus: 
* uv.x = atan(p.x, -p.z) * M_1_2PI + 0.5f;
* - Gives us the angle of a point (p.x, -p.z) on a plane laying on the X and Z axis
* - The result is positive when x > 0, negative when x < 0
* - The " * M_1_2PI + 0.5f" terms normalize the angle result from [-pi, pi] -> [-0.5, 0.5] -> [0, 1] == UV.x
* 	
* uv.y = acos(p.y) * M_1_PI;
* - Gives us the angle on [pi, 0], w.r.t the y coordinate [-1, 1] of our direction
* - The M_1_PI term normalizes this to [1, 0]
* 	-> y = -1 -> Looking straight down -> UV.y = 1 -> Bottom of the texture
*/
vec2 WorldDirToSphericalUV(vec3 unnormalizedDirection)
{
	const vec3 dir = normalize(unnormalizedDirection);

	vec2 uv;

	// Note: Reverse atan variables to change env. map orientation about y
	uv.x = atan(dir.x, -dir.z) * M_1_2PI + 0.5f; // atan == atan2 in HLSL

	uv.y = acos(dir.y) * M_1_PI; // Note: Use -p.y for (0,0) bottom left UVs, +p.y for (0,0) top left UVs

	return uv;
}


// Convert spherical map [0,1] UVs to a normalized world-space direction.
// Note: 0 rotation about Y == Z+ == UV [0, y] / [1, y]
// UV [0.5, 0.5] (i.e. middle of the spherical map) = Z-
vec3 SphericalUVToWorldDir(vec2 uv)
{
	// Note: Currently untested in GLSL, but known good in HLSL
	const float phi	= M_PI * uv.y;
	const float theta = M_2PI - (M_2PI * uv.x);

	float sinPhi = sin(phi);

	vec3 dir;
	dir.x = sinPhi * sin(theta);
	dir.y = cos(phi);
	dir.z = sinPhi * cos(theta);

	return normalize(dir);
}



// Converts a RHCS world-space direction to a LHCS cubemap sample direction.
vec3 WorldToCubeSampleDir(vec3 worldDir)
{
	return vec3(worldDir.x, worldDir.y, -worldDir.z);
}


// Convert a non-linear depth buffer value in [0,1] to a linear depth in [near, far] (eye space)
float ConvertNonLinearDepthToLinear(const float near, const float far, const float nonLinearDepth)
{
	float cubemapDepth_NDC = (2.0 * nonLinearDepth) - 1.0;
	float cubemapDepth_linear = (2.0 * near * far) / (far + near - cubemapDepth_NDC * (far - near));

	return cubemapDepth_linear;
}


// Convert a linear depth in [near, far] (eye space) to a non-linear depth buffer value in [0,1]
float ConvertLinearDepthToNonLinear(const float near, const float far, const float depthLinear)
{
	const float depthNDC = (((2.0 * near * far) / depthLinear) - far - near) / (near - far);
	const float depthNonLinear = (depthNDC + 1.0) / 2.0;

	return depthNonLinear;
}


// Transform pixel coordintes ([0, xRes), [0, yRes)) to [0,1] screen-space UVs.
//	offset: Use this if your pixel coords are centered in the pixel. NOTE: By default, OpenGL pixel centers (e.g. as 
//		found in gl_FragCoord) are located at half-pixel centers (e.g. (0.5, 0.5) is the top-left pixel in SaberEngine). 
//		Thus if using gl_FragCoord, you'll likely want a vec2(0,0) offset
//	doFlip: Use true if reading from a flipped FBO (e.g. GBuffer), false if the image being sampled has the correct
//		orientation
vec2 PixelCoordsToUV(vec2 pixelXY, vec2 screenWidthHeight, vec2 offset = vec2(0.5f, 0.5f), bool doFlip = true)
{
	vec2 screenUV = (vec2(pixelXY) + offset) / screenWidthHeight;
	if (doFlip)
	{
		screenUV.y = 1.f - screenUV.y;
	}
	return screenUV;
}


vec3 GetWorldPos(vec2 screenUV, float nonLinearDepth, mat4 invViewProjection)
{
	vec2 ndcXY = (screenUV * 2.f) - vec2(1.f, 1.f); // [0,1] -> [-1, 1]

	// Flip the Y coordinate so we can get back to the NDC that GLSL expects.
	// OpenGL uses a RHCS in view space, but LHCS in NDC. Flipping the Y coordinate here effectively reverses the Z axis
	// to account for this change of handedness.
	ndcXY.y *= -1;

	const vec4 ndcPos = vec4(ndcXY.xy, nonLinearDepth, 1.f);

	vec4 result = invViewProjection * ndcPos;
	return result.xyz / result.w; // Apply the perspective division
}

#endif // SABER_GLOBALS