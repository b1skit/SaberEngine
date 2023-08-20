#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


void main()
{	
	const vec2 screenUV = GetScreenUV(gl_FragCoord.xy, g_renderTargetResolution.xy);
	
	// Sample textures once inside the main shader flow, and pass the values as required:
	const vec4 linearAlbedo = texture(GBufferAlbedo, screenUV);
	const vec3 worldNormal = texture(GBufferWNormal, screenUV).xyz;
	const vec4 MatRMAO = texture(GBufferRMAO, screenUV);
	const vec4 matProp0 = texture(GBufferMatProp0, screenUV); // .rgb = F0 (Surface response at 0 degrees), .a = Phong exponent

	// Reconstruct the world position:
	const float nonLinearDepth = texture(GBufferDepth, screenUV).r;
	const vec4 worldPos = vec4(GetWorldPos(screenUV, nonLinearDepth, g_invViewProjection), 1.f);

	const vec3 lightWorldDir = normalize(g_lightWorldPos - worldPos.xyz);
	const vec3 lightViewDir = normalize((g_view * vec4(lightWorldDir, 0.0)).xyz);

	// Cube-map shadows:
	const float NoL = max(0.0, dot(worldNormal, lightWorldDir));
	const vec3 lightToFrag = worldPos.xyz - g_lightWorldPos; // Cubemap sampler dir length matters, so can't use -fragToLight
	const float shadowFactor = GetShadowFactor(lightToFrag, CubeMap0, NoL);

	// Factor in light attenuation:
	const float lightAtten = LightAttenuation(worldPos.xyz, g_lightWorldPos);
	const vec3 fragLight = g_lightColorIntensity * lightAtten;

	FragColor = ComputePBRLighting(
		linearAlbedo, 
		worldNormal, 
		MatRMAO, 
		worldPos, 
		matProp0.rgb, 
		NoL, 
		lightWorldDir, 
		fragLight, 
		shadowFactor, 
		g_view);
} 