#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"

// Built-in input variables:
//layout(pixel_center_integer) in vec4 gl_FragCoord; //  Fragment in window-space location: window-relative (x,y,z,1/w)
// in bool gl_FrontFacing;
// in vec2 gl_PointCoord;


#if defined AMBIENT_IBL

uniform int maxMipLevel; // Largest mip level in the PMREM cube map texture (CubeMap1). Uploaded during ImageBasedLight setup

void main()
{	
	// Note: All PBR calculations are performed in linear space
	// However, we use sRGB-format textures, getting the sRGB->Linear transformation for free when writing our GBuffer
	// for sRGB-format inputs (eg. MatAlbedo, ... and?) so no need to degamma MatAlbedo here
	const vec4 linearAlbedo	= texture(GBufferAlbedo, data.uv0);

	const vec3 worldNormal = texture(GBufferWNormal, data.uv0).xyz;
	const vec4 MatRMAO = texture(GBufferRMAO, data.uv0.xy);
	const vec4 worldPosition = texture(GBufferWPos, data.uv0.xy);
	const vec4 matProp0	= texture(GBufferMatProp0, data.uv0.xy); // .rgb = F0 (Surface response at 0 degrees)

	const float AO = MatRMAO.b;
	float metalness	= MatRMAO.y;

	vec4 viewPosition = in_view * worldPosition;					// View-space position
	vec3 viewEyeDir	= normalize(-viewPosition.xyz);					// View-space eye/camera direction
	vec3 viewNormal	= normalize(in_view * vec4(worldNormal, 0)).xyz;// View-space surface MatNormal

	float NoV = max(0.0, dot(viewNormal, viewEyeDir) );

	vec3 F0	= matProp0.rgb; // .rgb = F0 (Surface response at 0 degrees)
	F0 = mix(F0, linearAlbedo.rgb, metalness); 
	// Linear interpolation: x, y, using t=[0,1]. Returns x when t=0 -> Blends towards MatAlbedo for metals

//	vec3 fresnel_kS = FresnelSchlick(NoV, F0); // Doesn't quite look right: Use FresnelSchlick_Roughness() instead
	vec3 fresnel_kS	= FresnelSchlick_Roughness(NoV, F0, MatRMAO.x);
	vec3 k_d = 1.0 - fresnel_kS;	

	// Sample the diffuse irradiance from our prefiltered irradiance environment map:
	vec3 irradiance	= texture(CubeMap0, worldNormal).xyz * AO;


	// Get the specular reflectance term:
	vec3 worldView = normalize(cameraWPos - worldPosition.xyz);	// Direction = Point -> Eye
	vec3 worldReflection = normalize(reflect(-worldView, worldNormal));

	vec2 BRDF = texture(Tex7, vec2(max(NoV, 0.0), MatRMAO.x) ).rg; // Sample our generated BRDF Integration map
	vec3 specular = textureLod(CubeMap1, worldReflection, MatRMAO.x * maxMipLevel).xyz * ((fresnel_kS * BRDF.x) + BRDF.y);


	// FragColor = vec4((linearAlbedo.rgb * irradiance * k_d + specular), 1.0); // Note: Omitted the "/ PI" factor here
	// OLD:	FragColor = vec4((FragColor.rgb * irradiance * k_d + specular) * AO / M_PI, 1.0); // Note: Omitted the "/ PI" factor here

	const float ev100 = GetEV100FromExposureSettings(CAM_APERTURE, CAM_SHUTTERSPEED, CAM_SENSITIVITY);
	const float exposure = Exposure(ev100);
	FragColor = vec4((linearAlbedo.rgb * irradiance * k_d + specular) * exposure, 1.0); // Note: Omitted the "/ PI" factor here
}


#else
// AMBIENT_COLOR: No IBL found, fallback to using an ambient color

void main()
{	
	float AO = texture(GBufferRMAO, data.uv0.xy).b;

	// Phong ambient contribution:
	FragColor	= texture(GBufferAlbedo, data.uv0.xy) * vec4(lightColor, 1) * AO;	
}

#endif