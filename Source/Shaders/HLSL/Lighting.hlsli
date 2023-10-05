// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_LIGHTING
#define SABER_LIGHTING

#include "MathConstants.hlsli"


// Compute the dominant direction for sampling a Disney diffuse retro-reflection lobe from the IEM probe.
// Based on listing 23 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
float3 GetDiffuseDominantDir(float3 N, float3 V, float NoV, float roughness)
{
	const float a = 1.02341f * roughness - 1.51174f;
	const float b = -0.511705f * roughness + 0.755868f;
	const float lerpFactor = saturate((NoV * a + b) * roughness);
	
	return lerp(N, V, lerpFactor); // Don't normalize as this vector is for sampling a cubemap
}


// Compute the dominant direction for sampling the microfacet GGX-based specular lobe via the PMREM probe.
// Based on listing 21 & 22 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
float3 GetSpecularDominantDir(float3 N, float3 R, float NoV, float roughness)
{
//#define GSMITH_CORRELATED
//#define GSMITH_UNCORRELATED
#if defined(GSMITH_CORRELATED)
	const float lerpFactor = 
		pow(1.f - NoV, 10.8649f) * (1.f - 0.298475f * log(39.4115f - 39.0029f * roughness)) + 
		0.298475f * log(39.4115f - 39.0029f * roughness);
	return lerp(N, R, lerpFactor);
	
#elif defined(GSMITH_UNCORRELATED)
	const float lerpFactor = 
		0.298475f * NoV * log(39.4115f - 39.0029f * roughness) + 
		(0.385503f - 0.385503f * NoV) * log(13.1567f - 12.2848f * roughness);
	return lerp (N, R, lerpFactor );
#else
	// Frostbite simple approximation
	const float smoothness = saturate(1.f - roughness);
	const float lerpFactor = smoothness * (sqrt(smoothness) + roughness);
	return lerp(N, R, lerpFactor); // Note: Not normalized, as we're (currently) sampling from a cubemap
#endif
}


// Map linear roughness to "perceptually linear" roughness. 
// Perceptually linear roughness results in a linear-appearing transition from smooth to rough surfaces.
// As per p.13 of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al., we use the squared roughness
// remapping
float RemapRoughness(float linearRoughness)
{
	return linearRoughness * linearRoughness;
}


// Compute the half-direction vector: The normal of the microsurface that scatters light from an incident direction I
// to an outgoing direction O (E.g. I = light dir L, O = view dir V)
// Note: I points towards the point of reflection, O points away in the outgoing direction
// Based on section 4.1 (p.4) of "Microfacet Models for Refraction through Rough Surfaces", Walter et al.
float3 ComputeH(float3 I, float3 O)
{
	return I + O;
}

float3 ComputeNormalizedH(float3 I, float3 O)
{
	return normalize(ComputeH(I, O));
}


// Specular D is the normal distribution function (NDF), which approximates the surface area of microfacets aligned with
// the halfway vector between the light and view directions.
// As per Disney this is the GGX/Trowbridge-Reitz NDF, with their roughness reparameterization of alpha = roughness^2
float SpecularD(float roughness, float NoH)
{
	const float alpha = roughness * roughness; // Disney reparameterization: alpha = roughness^2
	const float alpha2 = alpha * alpha;
	const float NoH2 = NoH * NoH;
	
	return alpha2 / max((M_PI * pow((NoH2 * (alpha2 - 1.f) + 1.f), 2.f)), FLT_MIN); // == 1/pi when roughness = 1
}


// Compute the F_D90 term (i.e. reflectivity at grazing angles) for the Schlick Fresnel approximation used in our
// Cook-Torrance microfacet specular BRDF. 
//	theta_d = LoH, the cosine of the angle between the light vector, and the micronormal (aka. the half vector).
// Based on equation 5 (p.9) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al; and 
// section 5.3 (p.14) of "Physically Based Shading at Disney", Burley.
float ComputeF90(float roughness, float LoH)
{
	return 0.5f + 2.f * roughness * LoH * LoH;
}


// Fresnel function F (Shlick approximation).
// Describes the amount of light reflected from a (smooth) surface at the interface between 2 media.
// f0 = Reflectance at normal incidence. 
//	f0 = (n_1 - n_2)^2 / (n_1 + n_2)^2, with n_i = the material's index of refraction (IOR). 
//	When one media is air, which has an IOR ~= 1, f0 = (n-1)^2 / (n+1)^2
// f90 = Maximum reflectance (i.e. at grazing incidence, when the normal and ray are 90 degrees apart)
// u = cosine of the angle between the surface normal N and the incident ray
float3 FresnelSchlickF(in float3 f0, in float f90, in float u)
{
	// Schuler's solution for specular micro-occlusion.
	// derived from f0 (which is itself derived from the diffuse color), based on the knowledge that no real material
	// has a reflectance < 2%. Values of reflectance < 0.02 are assumed to be the result of pre-baked occlusion, and 
	// used to smoothly decrease the Fresnel reflectance contribution
	// f90 = saturate(50.0 * dot( fresnel0 , 0.33) );
	
	return f0 + (f90 - f0) * pow(1.f - u, 5.f);
}


// GGX Microfacet distribution function
float GeometrySchlickGGX(float NoV, float roughness)
{
	return NoV / ((NoV * (1.f - roughness)) + roughness);
}


// Bidirectional geometry shadow-masking function G.
// Describes the fraction of the microsurface normal visible in both the incoming and outgoing direction.
// This is the Smith G, as per section 5.1 of "Microfacet Models for Refraction through Rough Surfaces (Walter et al).
// Bidirectional shadow masking is approximated as the product of 2 monodirectional shadowing terms
float GeometryG(float NoV, float NoL, float roughness)
{
	const float ggx1 = GeometrySchlickGGX(NoV, roughness);
	const float ggx2 = GeometrySchlickGGX(NoL, roughness);
	
	return ggx1 * ggx2;
}


// Note: The original Disney diffuse model is not energy conserving. This implementation from Frostbite is a 
// modification that renormalizes it to make it _almost_ energy conserving
// Based on
float FrostbiteDisneyDiffuse(float NoV, float NoL, float LoH, float linearRoughness)
{
	const float energyBias		= lerp(0.f, 0.5f, linearRoughness);
	const float energyFactor	= lerp(1.f, 1.f / 1.51f, linearRoughness);
	const float fd90			= energyBias + 2.f * LoH * LoH * linearRoughness;
	const float3 f0				= float3(1.f, 1.f, 1.f);
	const float lightScatter	= FresnelSchlickF(f0, fd90, NoL).r;
	const float viewScatter		= FresnelSchlickF(f0, fd90, NoV).r;
	
	return lightScatter * viewScatter * energyFactor;
}


#endif