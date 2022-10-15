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
	vec2 uvs		= vec2(gl_FragCoord.x / screenParams.x, gl_FragCoord.y / screenParams.y); // [0, xRes/yRes] -> [0,1]

	// Sample textures once inside the main shader flow, and pass the values as required:
	vec4 linearAlbedo = texture(GBufferAlbedo, uvs);
	vec3 worldNormal = texture(GBufferWNormal, uvs).xyz;
	vec4 MatRMAO = texture(GBufferRMAO, uvs);
	vec4 worldPosition = texture(GBufferWPos, uvs);
	vec4 matProp0 = texture(GBufferMatProp0, uvs); // .rgb = F0 (Surface response at 0 degrees), .a = Phong exponent

	vec3 lightWorldDir = normalize(lightWorldPos - worldPosition.xyz);
	vec3 lightViewDir = normalize((g_view * vec4(lightWorldDir, 0.0)).xyz);

	// Cube-map shadows:
	float NoL				= max(0.0, dot(worldNormal, lightWorldDir));
	vec3 lightToFrag		= worldPosition.xyz - lightWorldPos; // Cubemap sampler direction length matters, so we can't use -fragToLight
	float shadowFactor		= GetShadowFactor(lightToFrag, CubeMap0, NoL);

	// Factor in light attenuation:
	float lightAtten		= LightAttenuation(worldPosition.xyz, lightWorldPos);
	vec3 fragLight			= lightColor * lightAtten;

	FragColor = ComputePBRLighting(
		linearAlbedo, 
		worldNormal, 
		MatRMAO, 
		worldPosition, 
		matProp0.rgb, 
		NoL, 
		lightWorldDir, 
		lightViewDir, 
		fragLight, 
		shadowFactor, 
		g_view);
} 