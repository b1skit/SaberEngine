#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


void main()
{
	// Sample textures once inside the main shader flow, and pass the values as required:
	vec4 linearAlbedo = texture(GBufferAlbedo, vOut.uv0.xy); // PBR calculations are performed in linear space
	vec3 worldNormal = texture(GBufferWNormal, vOut.uv0.xy).xyz;
	vec4 MatRMAO = texture(GBufferRMAO, vOut.uv0.xy);
	vec4 worldPosition = texture(GBufferWPos, vOut.uv0.xy);
	vec4 matProp0 = texture(GBufferMatProp0, vOut.uv0.xy); // .rgb = F0 (Surface response at 0 degrees)

	// Directional light direction is packed into the light position
	const vec3 keylightWorldDir = g_lightWorldPos;

	// Read from 2D shadow map:
	float NoL = max(0.0, dot(worldNormal, keylightWorldDir));

	vec3 shadowPos = (g_shadowCam_VP * worldPosition).xyz;

	float shadowFactor = GetShadowFactor(shadowPos, Depth0, NoL);

	// Note: Keylight doesn't have attenuation
	FragColor = ComputePBRLighting(
		linearAlbedo, 
		worldNormal, 
		MatRMAO, 
		worldPosition, 
		matProp0.rgb, 
		NoL, 
		keylightWorldDir, 
		g_lightColorIntensity, 
		shadowFactor, 
		g_view);
} 