#version 430 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"

// Built-in input variables:
// in vec4 gl_FragCoord; //  location of the fragment in window space. 
// in bool gl_FrontFacing;
// in vec2 gl_PointCoord;


void main()
{	
	FragColor					= texture(albedo, data.uv0.xy);
	vec3 worldNormal			= WorldNormalFromTexture(normal, data.uv0.xy, data.TBN);

	// Ambient:
	vec4 ambientContribution	= FragColor * vec4(ambientColor, 1);

	// Diffuse:
	vec4 diffuseContribution	= vec4( LambertianDiffuse(FragColor.xyz, worldNormal, lightColor, keylightWorldDir) , 1);

	// NOTE: keylightWorldDir is Key light's -forward direction (This shader only supports the keylight)

	// Shadow:
	float NoL					= max(0.0, dot(data.vertexWorldNormal, keylightWorldDir));
	float shadowFactor			= GetShadowFactor(data.shadowPos, shadowDepth, NoL);

	// Final result:
	FragColor = ambientContribution + (diffuseContribution * shadowFactor);
} 