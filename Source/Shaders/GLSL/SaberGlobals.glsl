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

// Gamma = 1.0 / 2.2:
#define GAMMA vec3(0.45454545454545454545454545454545454545, 0.45454545454545454545454545454545454545, 0.45454545454545454545454545454545454545)


// TODO: Make these camera params user-controllable

// f/stops == focal length / diameter of aperture. Commonly 1.4, 2, 2.8, 4, 5.6, 8, 11, 16. Lower = more exposure.
#define CAM_APERTURE 0.2 // == 5m / 25mm
#define CAM_SHUTTERSPEED 0.01 // == 1.0/100.0
#define CAM_SENSITIVITY 100.0 // ISO


mat3 AssembleTBN(const vec3 faceNormal, const vec4 localTangent, const mat4 model)
{
	// To rotate normal vectors, we must the transpose of the inverse of the model matrix, incase we have a
	// non-uniform scaling factor
	// https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/geometry/transforming-normals
	const mat4 transposeInverseModel = transpose(inverse(model));

	const vec3 worldTangent	= (transposeInverseModel * vec4(localTangent.xyz, 0)).xyz;

	// localTangent.w == 1.0 or -1.0
	const vec3 localBitangent = cross(faceNormal.xyz, localTangent.xyz) * localTangent.w;
	const vec3 worldBitangent = (transposeInverseModel * vec4(localBitangent, 0)).xyz;

	const vec3 worldFaceNormal = (transposeInverseModel * vec4(faceNormal, 0)).xyz;
	
	return mat3(worldTangent, worldBitangent, worldFaceNormal);
}


vec3 WorldNormalFromTextureNormal(vec3 texNormal, mat3 TBN)
{
	texNormal = normalize((texNormal * 2.0) - 1.0); // Transform [0,1] -> [-1,1]
	return normalize(TBN * texNormal);
}


// Linearize an sRGB gamma curved color value
vec3 Degamma(vec3 sRGB)
{
	return pow(sRGB, vec3(2.2, 2.2, 2.2));
}


// Apply Gamma correction to a linear color value
vec3 Gamma(vec3 linearColor)
{
	return pow(linearColor, GAMMA);
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


vec2 GetScreenUV(vec2 pixelXY, vec2 screenWidthHeight)
{
	vec2 screenUV = pixelXY / screenWidthHeight;
	screenUV.y = 1.f - screenUV.y;
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