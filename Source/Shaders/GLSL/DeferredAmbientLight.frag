// © 2023 Adam Badke. All rights reserved.
#version 460
#define SABER_VEC4_OUTPUT
#define READ_GBUFFER

#include "SaberCommon.glsl"
#include "SaberLighting.glsl"
#include "Transformations.glsl"
#include "GBufferCommon.glsl"
#include "UVUtils.glsl"


// Compute diffuse AO factor
// fineAO = AO from texture maps
float ComputeDiffuseAO(float fineAO)
{
	return fineAO;
}


// Compute the Frostbite specular AO factor
// Based on listing 26 (p.77) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// fineAO = AO from texture maps
float ComputeSpecularAO(float NoV, float remappedRoughness, float fineAO)
{
	const float totalAO = fineAO;
	return clamp(pow(NoV + totalAO, exp2(-16.f * remappedRoughness - 1.f)) - 1.f + fineAO, 0.f, 1.f);
}


// Compute a mip level for sampling the PMREM texture, using the remapped roughness
// Based on listing 63 (p.68) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// Note: maxMipLevel = Total number of mips - 1
float RemappedRoughnessToMipLevel(float remappedRoughness, float maxMipLevel)
{
	return sqrt(remappedRoughness) * maxMipLevel;
}

// Compute a mip level for sampling the PMREM texture, using the linear roughness
// Gives the same result as RemappedRoughnessToMipLevel
float LinearRoughnessToMipLevel(float linearRoughness, float maxMipLevel)
{
	return linearRoughness * maxMipLevel;
}


// Compute the dominant direction for sampling a Disney diffuse retro-reflection lobe from the IEM probe.
// Based on listing 23 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
vec3 GetDiffuseDominantDir(vec3 N, vec3 V, float NoV, float remappedRoughness)
{
	const float a = 1.02341f * remappedRoughness - 1.51174f;
	const float b = -0.511705f * remappedRoughness + 0.755868f;
	const float lerpFactor = clamp((NoV * a + b) * remappedRoughness, 0.f, 1.f);
	
	return mix(N, V, lerpFactor); // Note: Not normalized, as we're (currently) sampling from a cubemap
}


// Image-based lights use luminance units, as per p.25 Moving Frostbite to Physically Based Rendering 3.0", 
// Lagarde et al.
// Lv = lm/m^2.cr = cd/m^2)
// Lv = Luminance, lm = Lumens, sr = steradians, cd = Candela
// Based on listing 24 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
vec3 GetDiffuseIBLContribution(vec3 N, vec3 V, float NoV, float remappedRoughness)
{
	const float diffuseScale = _AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.z;
	
	const vec3 dominantN = GetDiffuseDominantDir(N, V, NoV, remappedRoughness);
	
	const vec3 diffuseLighting = texture(CubeMap0,WorldToCubeSampleDir(dominantN)).rgb; // IEM

	const float fDiffuse = texture(Tex7, vec2(NoV, remappedRoughness), 0).z;
	
	return diffuseLighting * fDiffuse * diffuseScale;
}


// Compute the dominant direction for sampling the microfacet GGX-based specular lobe via the PMREM probe.
// Based on listing 21 & 22 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
vec3 GetSpecularDominantDir(vec3 N, vec3 R, float NoV, float remappedRoughness)
{
//#define GSMITH_CORRELATED
//#define GSMITH_UNCORRELATED
#if defined(GSMITH_CORRELATED)
	const float lerpFactor = 
		pow(1.f - NoV, 10.8649f) * (1.f - 0.298475f * log(39.4115f - 39.0029f * remappedRoughness)) + 
		0.298475f * log(39.4115f - 39.0029f * remappedRoughness);
	return mix(N, R, lerpFactor);
	
#elif defined(GSMITH_UNCORRELATED)
	const float lerpFactor = 
		0.298475f * NoV * log(39.4115f - 39.0029f * remappedRoughness) + 
		(0.385503f - 0.385503f * NoV) * log(13.1567f - 12.2848f * remappedRoughness);
	return mix(N, R, lerpFactor );
#else
	// Frostbite simple approximation
	const float smoothness = clamp(1.f - remappedRoughness, 0.f, 1.f);
	const float lerpFactor = smoothness * (sqrt(smoothness) + remappedRoughness);
	return mix(N, R, lerpFactor); // Note: Not normalized, as we're (currently) sampling from a cubemap
#endif
}


// Based on listing 24 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
vec3 GetSpecularIBLContribution(
	vec3 N, vec3 R, vec3 V, float NoV, float linearRoughness, float remappedRoughness, vec3 blendedF0)
{
	const float maxPMREMMipLevel = _AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.x;
	const float dfgTexWidthHeight = _AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.y;
	const float specScale = _AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.w;

	const vec3 dominantR = GetSpecularDominantDir(N, R, NoV, remappedRoughness);
	
	// Rebuild the function:
	// L * D * (f0 * GVis * (1.f - Fc) + GVis * Fc) * (cosTheta / (4.f * NoL * NoV))
	NoV = max(NoV, 0.5f / dfgTexWidthHeight);
	
	const float mipSampleLevel = LinearRoughnessToMipLevel(linearRoughness, maxPMREMMipLevel);

	const vec3 H = ComputeNormalizedH(dominantR, V);
	const float LoH = clamp(dot(dominantR, H), 0.f, 1.f);
	
	const float f90 = ComputeF90(remappedRoughness, LoH);
	
	const vec3 preIntegratedLD = textureLod(CubeMap1, WorldToCubeSampleDir(dominantR), mipSampleLevel).rgb;

	// Sample the pre-integrated DFG texture
	//	Fc = (1.f - LoH)^5
	//	PreIntegratedDFG.r = GVis * (1.f - Fc)
	//	PreIntegratedDFG.g = GVis * Fc	
	const vec2 preIntegratedDFG = texture(Tex7, vec2(NoV, remappedRoughness), 0).xy;
	
	// LD * (f0 * GVis * (1.f - Fc) + GVis * Fc * f90)
	return preIntegratedLD * (blendedF0 * preIntegratedDFG.r + f90 * preIntegratedDFG.g) * specScale;
}



void main()
{	
	const GBuffer gbuffer = UnpackGBuffer(vOut.uv0.xy);

	// Reconstruct the world position:
	const vec4 worldPos = vec4(GetWorldPos(vOut.uv0.xy, gbuffer.NonLinearDepth, _CameraParams.g_invViewProjection), 1.f);

	const vec3 V = normalize(_CameraParams.g_cameraWPos.xyz - worldPos.xyz); // point -> camera
	const vec3 N = normalize(gbuffer.WorldNormal);
	
	const float NoV = clamp(dot(gbuffer.WorldNormal, V), 0.f, 1.f);
	
	const float linearRoughness = gbuffer.LinearRoughness;
	const float remappedRoughness = RemapRoughness(linearRoughness);
		
	const vec3 diffuseIlluminance = GetDiffuseIBLContribution(N, V, NoV, remappedRoughness);
	const float diffuseAO = ComputeDiffuseAO(gbuffer.AO);
	
	const vec3 dielectricSpecular = gbuffer.MatProp0.rgb;
	const vec3 blendedF0 = ComputeBlendedF0(dielectricSpecular, gbuffer.LinearAlbedo, gbuffer.LinearMetalness);
	
	const vec3 diffuseColor = ComputeDiffuseColor(gbuffer.LinearAlbedo, blendedF0, gbuffer.LinearMetalness);
	
	const vec3 R = reflect(-V, N);
	
	const vec3 specularIlluminance = 
		GetSpecularIBLContribution(N, R, V, NoV, linearRoughness, remappedRoughness, blendedF0);
	const float specularAO = ComputeSpecularAO(NoV, remappedRoughness, gbuffer.AO);
	
	const vec3 combinedContribution = 
		(diffuseColor * diffuseIlluminance * diffuseAO) + (specularIlluminance * specularAO);
	// Note: We're omitting the pi term in the albedo
	
	FragColor = vec4(combinedContribution, 0.f);
}