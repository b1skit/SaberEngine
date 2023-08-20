#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


void main()
{
	const vec2 screenUV = vOut.uv0.xy; // Directional light is drawn with a fullscreen quad

	// Sample textures once inside the main shader flow, and pass the values as required:
	vec4 linearAlbedo = texture(GBufferAlbedo, screenUV); // PBR calculations are performed in linear space
	vec3 worldNormal = texture(GBufferWNormal, screenUV).xyz;
	vec4 MatRMAO = texture(GBufferRMAO, screenUV);
	vec4 matProp0 = texture(GBufferMatProp0, screenUV); // .rgb = F0 (Surface response at 0 degrees)

	// Reconstruct the world position:
	const float nonLinearDepth = texture(GBufferDepth, screenUV).r;
	const vec4 worldPos = vec4(GetWorldPos(screenUV, nonLinearDepth, g_invViewProjection), 1.f);

	// Directional light direction is packed into the light position
	const vec3 keylightWorldDir = g_lightWorldPos;

	// Read from 2D shadow map:
	float NoL = max(0.0, dot(worldNormal, keylightWorldDir));

	vec3 shadowPos = (g_shadowCam_VP * worldPos).xyz;

	float shadowFactor = GetShadowFactor(shadowPos, Depth0, NoL);

	// Note: Keylight doesn't have attenuation
	FragColor = ComputePBRLighting(
		linearAlbedo, 
		worldNormal, 
		MatRMAO, 
		worldPos, 
		matProp0.rgb, 
		NoL, 
		keylightWorldDir, 
		g_lightColorIntensity, 
		shadowFactor, 
		g_view);
} 