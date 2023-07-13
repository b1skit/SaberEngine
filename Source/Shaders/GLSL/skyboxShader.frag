#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


// Make our fragment coordinates ([0,xRes], [0,yRes]) match our uv (0,0) = top-left convention
layout(origin_upper_left) in vec4 gl_FragCoord;

void main()
{	
	vec4 ndcPosition;
	ndcPosition.xy	= ((2.0 * gl_FragCoord.xy) / g_skyboxTargetResolution.xy) - 1.0;
	ndcPosition.z	= 1.0;
	ndcPosition.w	= 1.0;
	
	const vec4 worldPos	= g_invViewProjection * ndcPosition;

	// Sample our equirectangular skybox projection:
	const vec3 sampleDir = worldPos.xyz;
	const vec2 sphericalUVs = WorldDirToSphericalUV(sampleDir); // Normalizes incoming sampleDir

	FragColor = texture(Tex0, sphericalUVs);
}