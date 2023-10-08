// © 2023 Adam Badke. All rights reserved.
#version 460
#define SABER_VEC4_OUTPUT
#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


void main()
{	
	const GBuffer gbuffer = UnpackGBuffer(vOut.uv0.xy);

	const float diffuseScale = g_maxPMREMMipDFGResScaleDiffuseScaleSpec.z;
	const float specScale = g_maxPMREMMipDFGResScaleDiffuseScaleSpec.w;

	// Reconstruct the world position:
	const vec3 worldPos = GetWorldPos(vOut.uv0.xy, gbuffer.NonLinearDepth, g_invViewProjection);

	const vec3 N = normalize(gbuffer.WorldNormal.xyz);
	const vec3 V = normalize(g_cameraWPos - worldPos); // World-space point -> camera direction
	
	const float NoV	= clamp(dot(N, V), FLT_EPSILON, 1.f); // Prevent NaNs at glancing angles

	const float linearRoughness = gbuffer.LinearRoughness;
	const float remappedRoughness = RemapRoughness(linearRoughness);

	vec3 F0	= gbuffer.MatProp0.rgb; // .rgb = F0 (Surface response at 0 degrees)
	F0 = mix(F0, gbuffer.LinearAlbedo, gbuffer.LinearMetalness); 
	// Linear interpolation: x, y, using t=[0,1]. Returns x when t=0 -> Blends towards MatAlbedo for metals

//	vec3 fresnel_kS = FresnelSchlickF(NoV, F0); // Doesn't quite look right: Use FresnelSchlick_Roughness() instead
	const vec3 fresnel_kS = FresnelSchlick_Roughness(NoV, F0, gbuffer.LinearRoughness);
	// TODO: Make this aligned with the DX12 version

	const vec3 k_d = 1.0 - fresnel_kS;	

	// Sample the diffuse irradiance from our prefiltered irradiance environment map.
	const vec3 diffuse = 
		texture(CubeMap0, WorldToCubeSampleDir(gbuffer.WorldNormal)).xyz * gbuffer.AO * diffuseScale;

	const vec3 R = reflect(-V, N);

	const float maxPMREMMipLevel = g_maxPMREMMipDFGResScaleDiffuseScaleSpec.x;

	// Sample our generated BRDF Integration map using the linear roughness
	const vec2 BRDF = texture(Tex7, vec2(max(NoV, 0.0), linearRoughness) ).rg; 

	const vec3 specular = textureLod(
		CubeMap1, 
		WorldToCubeSampleDir(R), 
		remappedRoughness * maxPMREMMipLevel).xyz * ((fresnel_kS * BRDF.x) + BRDF.y) * specScale;

	const vec3 combinedContribution = (gbuffer.LinearAlbedo * diffuse * k_d + specular);
	// Note: Omitted the "/ PI" factor here

	// Apply exposure:
	const vec3 exposedColor = ApplyExposure(combinedContribution, g_exposureProperties.x);

	FragColor = vec4(exposedColor, 0.f);
}