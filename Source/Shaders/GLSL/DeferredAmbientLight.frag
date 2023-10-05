// © 2023 Adam Badke. All rights reserved.
#version 460
#define SABER_VEC4_OUTPUT
#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


void main()
{	
	const vec2 screenUV = vOut.uv0.xy; // Ambient is drawn with a fullscreen quad

	const GBuffer gbuffer = UnpackGBuffer(screenUV);

	const float diffuseScale = g_maxPMREMMipDFGResScaleDiffuseScaleSpec.z;
	const float specScale = g_maxPMREMMipDFGResScaleDiffuseScaleSpec.w;

	// Reconstruct the world position:
	const vec4 worldPos = vec4(GetWorldPos(screenUV, gbuffer.NonLinearDepth, g_invViewProjection), 1.f);

	const vec4 viewPosition = g_view * worldPos;
	const vec3 viewEyeDir = normalize(-viewPosition.xyz);	// View-space eye/camera direction

	const mat3 viewRotationScale = mat3(g_view); // Ignore the translation
	const vec3 viewNormal = normalize(viewRotationScale * gbuffer.WorldNormal); // View-space surface MatNormal

	const float NoV = max(0.0, dot(viewNormal, viewEyeDir) );

	vec3 F0	= gbuffer.MatProp0.rgb; // .rgb = F0 (Surface response at 0 degrees)
	F0 = mix(F0, gbuffer.LinearAlbedo, gbuffer.LinearMetalness); 
	// Linear interpolation: x, y, using t=[0,1]. Returns x when t=0 -> Blends towards MatAlbedo for metals

//	vec3 fresnel_kS = FresnelSchlick(NoV, F0); // Doesn't quite look right: Use FresnelSchlick_Roughness() instead
	const vec3 fresnel_kS = FresnelSchlick_Roughness(NoV, F0, gbuffer.LinearRoughness);
	const vec3 k_d = 1.0 - fresnel_kS;	

	// Sample the diffuse irradiance from our prefiltered irradiance environment map.
	const vec3 diffuse = 
		texture(CubeMap0, WorldToCubeSampleDir(gbuffer.WorldNormal)).xyz * gbuffer.AO * diffuseScale;

	// Get the specular reflectance term:
	const vec3 worldView = normalize(worldPos.xyz - g_cameraWPos); // Incident direction: camera -> point
	vec3 worldReflection = normalize(reflect(worldView, gbuffer.WorldNormal));

	const float linearRoughness = gbuffer.LinearRoughness;
	const float remappedRoughness = RemapRoughnessIBL(linearRoughness);
	const float maxPMREMMipLevel = g_maxPMREMMipDFGResScaleDiffuseScaleSpec.x;

	// Sample our generated BRDF Integration map using the linear roughness
	const vec2 BRDF = texture(Tex7, vec2(max(NoV, 0.0), linearRoughness) ).rg; 

	const vec3 specular = textureLod(
		CubeMap1, 
		WorldToCubeSampleDir(worldReflection), 
		remappedRoughness * maxPMREMMipLevel).xyz * ((fresnel_kS * BRDF.x) + BRDF.y) * specScale;

	const vec3 combinedContribution = (gbuffer.LinearAlbedo * diffuse * k_d + specular); // Note: Omitted the "/ PI" factor here

	// Apply exposure:
	const vec3 exposedColor = ApplyExposure(combinedContribution, g_exposureProperties.x);

	FragColor = vec4(exposedColor, 1.0); // Note: Omitted the "/ PI" factor here
}