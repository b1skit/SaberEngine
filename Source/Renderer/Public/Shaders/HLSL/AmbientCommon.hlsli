// © 2024 Adam Badke. All rights reserved.
#ifndef SE_AMBIENT_COMMON
#define SE_AMBIENT_COMMON

#include "Lighting.hlsli"
#include "SaberCommon.hlsli"
#include "UVUtils.hlsli"

#include "../Common/LightParams.h"


ConstantBuffer<AmbientLightData> AmbientLightParams;

Texture2D<float4> DFG;
TextureCube<float4> CubeMapIEM;
TextureCube<float4> CubeMapPMREM;


// Combine AO terms: fineAO = from GBuffer textures, coarseAO = SSAO
float CombineAO(float fineAO, float coarseAO)
{
	return min(fineAO, coarseAO);
}


// Compute the Frostbite specular AO factor
// Based on listing 26 (p.77) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// fineAO = AO from texture maps
float ComputeSpecularAO(float NoV, float remappedRoughness, float fineAO)
{
	const float totalAO = fineAO;
	// Use pow(abs(f), e) to suppress warning X3571
	return saturate(pow(abs(NoV + totalAO), exp2(-16.f * remappedRoughness - 1.f)) - 1.f + fineAO);
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
float3 GetDiffuseDominantDir(float3 N, float3 V, float NoV, float remappedRoughness)
{
	const float a = 1.02341f * remappedRoughness - 1.51174f;
	const float b = -0.511705f * remappedRoughness + 0.755868f;
	const float lerpFactor = saturate((NoV * a + b) * remappedRoughness);
	
	return lerp(N, V, lerpFactor); // Note: Not normalized, as we're (currently) sampling from a cubemap
}


// Image-based lights use luminance units, as per p.25 Moving Frostbite to Physically Based Rendering 3.0", 
// Lagarde et al.
// Lv = lm/m^2.cr = cd/m^2)
// Lv = Luminance, lm = Lumens, sr = steradians, cd = Candela
// Based on listing 24 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
float3 GetDiffuseIBLContribution(float3 N, float3 V, float NoV, float remappedRoughness)
{
	static const float diffuseScale = AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.z;
	
	const float3 dominantN = GetDiffuseDominantDir(N, V, NoV, remappedRoughness);
	
	const float3 diffuseLighting = CubeMapIEM.Sample(WrapMinMagLinearMipPoint, WorldToCubeSampleDir(dominantN)).rgb; // IEM
	
	const float fDiffuse = DFG.SampleLevel(ClampMinMagLinearMipPoint, float2(NoV, remappedRoughness), 0).z;
	
	return diffuseLighting * fDiffuse * diffuseScale;
}


// Compute the dominant direction for sampling the microfacet GGX-based specular lobe via the PMREM probe.
// Based on listing 21 & 22 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
float3 GetSpecularDominantDir(float3 N, float3 R, float NoV, float remappedRoughness)
{
//#define GSMITH_CORRELATED
//#define GSMITH_UNCORRELATED
#if defined(GSMITH_CORRELATED)
	const float lerpFactor = 
		pow(1.f - NoV, 10.8649f) * (1.f - 0.298475f * log(39.4115f - 39.0029f * remappedRoughness)) + 
		0.298475f * log(39.4115f - 39.0029f * remappedRoughness);
	return lerp(N, R, lerpFactor);
	
#elif defined(GSMITH_UNCORRELATED)
	const float lerpFactor = 
		0.298475f * NoV * log(39.4115f - 39.0029f * remappedRoughness) + 
		(0.385503f - 0.385503f * NoV) * log(13.1567f - 12.2848f * remappedRoughness);
	return lerp (N, R, lerpFactor );
#else
	// Frostbite simple approximation
	const float smoothness = saturate(1.f - remappedRoughness);
	const float lerpFactor = smoothness * (sqrt(smoothness) + remappedRoughness);
	return lerp(N, R, lerpFactor); // Note: Not normalized, as we're (currently) sampling from a cubemap
#endif
}


// Based on listing 24 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
float3 GetSpecularIBLContribution(
	float3 N, float3 R, float3 V, float NoV, float linearRoughness, float remappedRoughness, float3 blendedF0)
{
	static const float maxPMREMMipLevel = AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.x;
	static const float dfgTexWidthHeight = AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.y;
	static const float specScale = AmbientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec.w;

	const float3 dominantR = GetSpecularDominantDir(N, R, NoV, remappedRoughness);
	
	// Rebuild the function:
	// L * D * (f0 * GVis * (1.f - Fc) + GVis * Fc) * (cosTheta / (4.f * NoL * NoV))
	NoV = max(NoV, 0.5f / dfgTexWidthHeight);
	
	const float mipSampleLevel = LinearRoughnessToMipLevel(linearRoughness, maxPMREMMipLevel);

	const float3 H = ComputeNormalizedH(dominantR, V);
	const float LoH = saturate(dot(dominantR, H));
	
	const float f90 = ComputeF90(remappedRoughness, LoH);
	
	const float3 preIntegratedLD =
		CubeMapPMREM.SampleLevel(WrapMinMagMipLinear, WorldToCubeSampleDir(dominantR), mipSampleLevel).rgb;
	
	// Sample the pre-integrated DFG texture
	//	Fc = (1.f - LoH)^5
	//	PreIntegratedDFG.r = GVis * (1.f - Fc)
	//	PreIntegratedDFG.g = GVis * Fc	
	const float2 preIntegratedDFG = DFG.SampleLevel(ClampMinMagLinearMipPoint, float2(NoV, remappedRoughness), 0).xy;
	
	// LD * (f0 * GVis * (1.f - Fc) + GVis * Fc * f90)
	return preIntegratedLD * (blendedF0 * preIntegratedDFG.r + f90 * preIntegratedDFG.g) * specScale;
}


struct AmbientLightingParams
{
	float3 WorldPosition;
	float3 V;			// Point -> camera
	float3 WorldNormal;
	
	float3 LinearAlbedo;
	
	float3 DielectricSpecular;
	float LinearMetalness;
	
	float LinearRoughness;
	float RemappedRoughness;
	
	float FineAO;		// From textures
	float CoarseAO;		// From SSAO
};


float3 ComputeAmbientLighting(AmbientLightingParams lightingParams)
{	
	const float3 blendedF0 = ComputeBlendedF0(
		lightingParams.DielectricSpecular,
		lightingParams.LinearAlbedo,
		lightingParams.LinearMetalness);
	
	const float3 diffuseColor =
		ComputeDiffuseColor(lightingParams.LinearAlbedo, blendedF0, lightingParams.LinearMetalness);

	const float NoV = saturate(dot(lightingParams.WorldNormal, lightingParams.V));
	
	const float3 diffuseIlluminance = GetDiffuseIBLContribution(
		lightingParams.WorldNormal,
		lightingParams.V, 
		NoV, 
		lightingParams.RemappedRoughness);
	
	const float3 R = reflect(-lightingParams.V, lightingParams.WorldNormal);
	
	const float3 specularIlluminance = GetSpecularIBLContribution(
		lightingParams.WorldNormal,
		R,
		lightingParams.V,
		NoV,
		lightingParams.LinearRoughness,
		lightingParams.RemappedRoughness,
		blendedF0);

	const float combinedAO = CombineAO(lightingParams.FineAO, lightingParams.CoarseAO);
	
	const float diffuseAO = combinedAO;
	
	const float specularAO = ComputeSpecularAO(NoV, lightingParams.RemappedRoughness, combinedAO);
	
	const float3 combinedContribution =
		(diffuseColor * diffuseIlluminance * diffuseAO) + (specularIlluminance * specularAO);
	// Note: We're omitting the pi term in the albedo
	
	return combinedContribution;
}

#endif // SE_AMBIENT_COMMON