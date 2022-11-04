#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_DEPTH
#include "SaberCommon.glsl"


in vec4 FragPos; // Projection space


void main()
{
	// TODO: Delete this fragment shader, and switch to using a samplerCubeShadow to compare "raw" shadow depths

	float lightDistance = length(FragPos.xyz - g_cubemapLightWorldPos);
	
	// Map to [0, 1]:
	const float shadowCamNear = g_cubemapShadowCamNearFar.y;
	lightDistance = lightDistance / shadowCamNear; // TODO: Correct for perspective and write a linear depth
	
	// write this as modified depth
	gl_FragDepth = lightDistance;
} 