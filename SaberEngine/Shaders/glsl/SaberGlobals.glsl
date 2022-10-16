#ifndef SABER_GLOBALS
#define SABER_GLOBALS

// Saber Engine Shader Globals
// Defines functions common to all shaders
//----------------------------------------



// Global defines:
//----------------
#define M_PI	3.1415926535897932384626433832795	// pi
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
	// To transpose normal vectors, we must the transpose of the inverse of the model matrix, incase we have a
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


// Convert a MatNormal sampled from a texture to an object-space MatNormal
vec3 ObjectNormalFromTexture(mat3 TBN, vec3 textureNormal)
{
	textureNormal	= normalize((textureNormal * 2.0) - 1.0);	// Transform [0,1] -> [-1,1]

	vec3 result		= normalize(TBN * textureNormal);

	return result;
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


// Convert a world-space direction to spherical (latitude-longitude) map [0,1] UVs.
vec2 WorldDirToSphericalUV(vec3 direction)
{
	const vec3 p = normalize(direction);

	vec2 uv;
	// Note: atan2 in HLSL
	uv.x = atan(p.x, -p.z) * M_1_2PI + 0.5f; // Note: Reverse atan variables to change env. map orientation about y
	uv.y = acos(-p.y) * M_1_PI; // Note: Use -y in GLSL, +y in HLSL

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

#endif // SABER_GLOBALS