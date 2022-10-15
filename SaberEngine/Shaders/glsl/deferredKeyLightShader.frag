#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"

// Built-in input variables:
//layout(pixel_center_integer) in vec4 gl_FragCoord; //  Location of the fragment in window space. (x,y,z,w) = window-relative (x,y,z,1/w)
// in bool gl_FrontFacing;
// in vec2 gl_PointCoord;


void main()
{
	// Sample textures once inside the main shader flow, and pass the values as required:
	vec4 linearAlbedo	= texture(GBufferAlbedo, data.uv0.xy); // PBR calculations are performed in linear space
	vec3 worldNormal	= texture(GBufferWNormal, data.uv0.xy).xyz;
	vec4 MatRMAO		= texture(GBufferRMAO, data.uv0.xy);
	vec4 worldPosition	= texture(GBufferWPos, data.uv0.xy);
	vec4 matProp0		= texture(GBufferMatProp0, data.uv0.xy); // .rgb = F0 (Surface response at 0 degrees)

	// Read from 2D shadow map:
	float NoL				= max(0.0, dot(worldNormal, keylightWorldDir));
	vec3 shadowPos			= (shadowCam_vp * worldPosition).xyz;
	float shadowFactor		= GetShadowFactor(shadowPos, Depth0, NoL);

	// Note: Keylight lightColor doesn't need any attenuation to be factored in
	FragColor = ComputePBRLighting(
		linearAlbedo, 
		worldNormal, 
		MatRMAO, 
		worldPosition, 
		matProp0.rgb, 
		NoL, 
		keylightWorldDir, 
		keylightViewDir, 
		lightColor, 
		shadowFactor, 
		g_view);
} 