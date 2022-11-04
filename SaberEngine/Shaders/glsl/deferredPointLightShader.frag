#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"

// Built-in input variables:
//layout(pixel_center_integer) in vec4 gl_FragCoord; //  Window space fragment location. (x,y,z,w) = window-relative (x,y,z,1/w)
// in bool gl_FrontFacing;
// in vec2 gl_PointCoord;

in vec4 gl_FragCoord;


void main()
{	
	const vec2 uvs = vec2(gl_FragCoord.x / g_targetResolution.x, gl_FragCoord.y / g_targetResolution.y); // [0, xRes/yRes] -> [0,1]
	
	// Sample textures once inside the main shader flow, and pass the values as required:
	const vec4 linearAlbedo = texture(GBufferAlbedo, uvs);
	const vec3 worldNormal = texture(GBufferWNormal, uvs).xyz;
	const vec4 MatRMAO = texture(GBufferRMAO, uvs);
	const vec4 worldPosition = texture(GBufferWPos, uvs);
	const vec4 matProp0 = texture(GBufferMatProp0, uvs); // .rgb = F0 (Surface response at 0 degrees), .a = Phong exponent

	const vec3 lightWorldDir = normalize(g_lightWorldPos - worldPosition.xyz);
	const vec3 lightViewDir = normalize((g_view * vec4(lightWorldDir, 0.0)).xyz);

	// Cube-map shadows:
	const float NoL = max(0.0, dot(worldNormal, lightWorldDir));
	const vec3 lightToFrag = worldPosition.xyz - g_lightWorldPos; // Cubemap sampler dir length matters, so can't use -fragToLight
	const float shadowFactor = GetShadowFactor(lightToFrag, CubeMap0, NoL);

	// Factor in light attenuation:
	const float lightAtten = LightAttenuation(worldPosition.xyz, g_lightWorldPos);
	const vec3 fragLight = g_lightColorIntensity * lightAtten;

	FragColor = ComputePBRLighting(
		linearAlbedo, 
		worldNormal, 
		MatRMAO, 
		worldPosition, 
		matProp0.rgb, 
		NoL, 
		lightWorldDir, 
		fragLight, 
		shadowFactor, 
		g_view);
} 