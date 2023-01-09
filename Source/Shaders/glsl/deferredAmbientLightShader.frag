#version 460 core

#define SABER_FRAGMENT_SHADER
#define SABER_VEC4_OUTPUT

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


#if defined AMBIENT_IBL

void main()
{	
	// Note: All PBR calculations are performed in linear space
	// However, we use sRGB-format textures, getting the sRGB->Linear transformation for free when writing our GBuffer
	// for sRGB-format inputs (eg. MatAlbedo, ... and?) so no need to degamma MatAlbedo here
	const vec4 linearAlbedo	= texture(GBufferAlbedo, vOut.uv0);

	const vec3 worldNormal = texture(GBufferWNormal, vOut.uv0).xyz;
	const vec4 MatRMAO = texture(GBufferRMAO, vOut.uv0.xy);
	const vec4 worldPosition = texture(GBufferWPos, vOut.uv0.xy);
	const vec4 matProp0 = texture(GBufferMatProp0, vOut.uv0.xy); // .rgb = F0 (Surface response at 0 degrees)

	const float AO = MatRMAO.b;
	float metalness	= MatRMAO.y;

	vec4 viewPosition = g_view * worldPosition; // View-space position
	vec3 viewEyeDir	= normalize(-viewPosition.xyz);	// View-space eye/camera direction
	vec3 viewNormal	= normalize(g_view * vec4(worldNormal, 0)).xyz; // View-space surface MatNormal

	float NoV = max(0.0, dot(viewNormal, viewEyeDir) );

	vec3 F0	= matProp0.rgb; // .rgb = F0 (Surface response at 0 degrees)
	F0 = mix(F0, linearAlbedo.rgb, metalness); 
	// Linear interpolation: x, y, using t=[0,1]. Returns x when t=0 -> Blends towards MatAlbedo for metals

//	vec3 fresnel_kS = FresnelSchlick(NoV, F0); // Doesn't quite look right: Use FresnelSchlick_Roughness() instead
	vec3 fresnel_kS	= FresnelSchlick_Roughness(NoV, F0, MatRMAO.x);
	vec3 k_d = 1.0 - fresnel_kS;	

	// Sample the diffuse irradiance from our prefiltered irradiance environment map.
	// Note: We must flip the Y component of our normal to compensate for our UV (0,0) top-left convention
	const vec3 diffuseCubeSampleDir = vec3(worldNormal.x, -worldNormal.y, worldNormal.z);
	vec3 irradiance	= texture(CubeMap0, diffuseCubeSampleDir).xyz * AO;

	// Get the specular reflectance term:
	// Note: We must flip the Y component of our reflected vector to compensate for our UV (0,0) top-left convention
	const vec3 worldView = normalize(g_cameraWPos - worldPosition.xyz); // Direction = Point -> Eye
	vec3 worldReflection = normalize(reflect(-worldView, worldNormal));
	worldReflection.y *= -1;

	vec2 BRDF = texture(Tex7, vec2(max(NoV, 0.0), MatRMAO.x) ).rg; // Sample our generated BRDF Integration map
	vec3 specular = textureLod(CubeMap1, worldReflection, MatRMAO.x * g_maxPMREMMip).xyz * ((fresnel_kS * BRDF.x) + BRDF.y);

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
	float AO = texture(GBufferRMAO, vOut.uv0.xy).b;

	// Phong ambient contribution:
	FragColor = texture(GBufferAlbedo, vOut.uv0.xy) * vec4(lightColor, 1) * AO;
}

#endif