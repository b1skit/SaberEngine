#ifndef SABER_GLOBALS
#define SABER_GLOBALS

// Saber Engine Shader Globals
// Defines functions common to all shaders
//----------------------------------------



// Global defines:
//----------------
#define M_PI		3.1415926535897932384626433832795	// pi
#define M_2PI       6.28318530717958647693		// 2pi
#define M_4PI       12.5663706143591729539		// 4pi
#define M_PI_2      1.57079632679489661923		// pi/2
#define M_PI_4      0.785398163397448309616		// pi/4
#define M_1_PI      0.318309886183790671538		// 1/pi
#define M_2_PI      0.636619772367581343076		// 2/pi
#define M_4_PI      1.27323954473516268615		// 4/pi
#define M_1_2PI     0.159154943091895335769		// 1/(2pi)
#define M_1_4PI     0.079577471545947667884		// 1/(4pi)
#define M_SQRTPI    1.77245385090551602730		// sqrt(pi)
#define M_2_SQRTPI  1.12837916709551257390		// 2/sqrt(pi)
#define M_SQRT2     1.41421356237309504880		// sqrt(2)
#define M_1_SQRT2   0.707106781186547524401		// 1/sqrt(2)

#define FLT_MAX		3.402823466e+38
#define FLT_MIN		1.175494351e-38


#if defined(READ_GBUFFER)
struct GBuffer
{
	vec3 LinearAlbedo;
	vec3 WorldNormal;

	float Roughness;
	float Metalness;
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
	gbuffer.Roughness = RMAO.r;
	gbuffer.Metalness = RMAO.g;
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


vec3 sRGBToLinear(vec3 srgbColor)
{
	// https://en.wikipedia.org/wiki/SRGB#Computing_the_transfer_function
	vec3 result = vec3(0, 0, 0);
	for (int c = 0; c < 3; c++)
	{
		result[c] = srgbColor[c] <= 0.04045 ? (srgbColor[c] / 12.92f) : pow((srgbColor[c] + 0.055f) / 1.055f, 2.4f);
	}
	return result;
}


vec4 sRGBToLinear(vec4 srgbColorWithAlpha)
{
	return vec4(sRGBToLinear(srgbColorWithAlpha.rgb), srgbColorWithAlpha.a);
}


vec3 LinearToSRGB(vec3 linearColor)
{
	// https://en.wikipedia.org/wiki/SRGB#Computing_the_transfer_function
	// Note: The 2 functions intersect at x = 0.0031308
	vec3 result = vec3(0, 0, 0);
	for (int c = 0; c < 3; c++)
	{
		result[c] = 
			linearColor[c] <= 0.0031308 ? 12.92f * linearColor[c] : 1.055f * pow(abs(linearColor[c]), 1.f / 2.4f) - 0.055f;
	}
	return result;
}


vec4 LinearToSRGB(vec4 linearColorWithAlpha)
{
	return vec4(LinearToSRGB(linearColorWithAlpha.rgb), linearColorWithAlpha.a);
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
// offset: Use this if your pixel coords are centered in the pixel. NOTE: By default, OpenGL pixel centers (e.g. as 
//	found in gl_FragCoord) are located at half-pixel centers (e.g. (0.5, 0.5) is the top-left pixel in SaberEngine). 
//	Thus if using gl_FragCoord, you'll likely want a vec2(0,0) offset
// doFlip: Use true if reading from a flipped FBO (e.g. GBuffer), false if the image being sampled hsa the correct
//	orientation
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