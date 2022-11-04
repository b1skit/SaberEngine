#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"

// Note: This shader uses the following built-in GLSL shader variables:
//in vec4 gl_FragCoord; //  Location of the fragment in window space. (x,y,z,w) = window-relative (x,y,z,1/w)
//struct gl_DepthRangeParameters
//{
//    float near;
//    float far;
//    float diff;	// far - near
//};
//uniform gl_DepthRangeParameters gl_DepthRange;


void main()
{	
	// Sample the cube map:
	vec4 ndcPosition;
	ndcPosition.xy	= ((2.0 * gl_FragCoord.xy) / g_targetResolution.xy) - 1.0;
	ndcPosition.z	= 1.0;
	ndcPosition.w	= 1.0;
	
	const vec4 worldPos	= g_invViewProjection * ndcPosition;


#if defined(CUBEMAP_SKY)
	FragColor = texture(CubeMap0, worldPos.xyz);

#else
	// Equirectangular skybox image:
	const vec3 sampleDir = worldPos.xyz;

	const vec2 sphericalUVs = WorldDirToSphericalUV(sampleDir);

	FragColor = texture(Tex0, sphericalUVs);
#endif	
}