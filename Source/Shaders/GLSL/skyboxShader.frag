#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"


void main()
{	
	vec2 screenUV = GetScreenUV(gl_FragCoord.xy, g_skyboxTargetResolution.xy);

	const float sampleDepth = 0.f; // Avoids distortion from the inverse view projection matrix
	const vec4 worldPos	= GetWorldPos(screenUV, sampleDepth, g_invViewProjection);
	
	const vec3 sampleDir = worldPos.xyz - g_cameraWPos.xyz; // The skybox is centered about the camera

	const vec2 sphericalUVs = WorldDirToSphericalUV(sampleDir); // Normalizes incoming sampleDir

	FragColor = texture(Tex0, sphericalUVs);
}