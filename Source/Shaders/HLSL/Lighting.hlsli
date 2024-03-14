// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_LIGHTING
#define SABER_LIGHTING

#include "MathConstants.hlsli"
#include "../Common/LightParams.h"


ConstantBuffer<LightParamsData> LightParams;


// Map linear roughness to "perceptually linear" roughness. 
// Perceptually linear roughness results in a linear-appearing transition from smooth to rough surfaces.
// As per p.13 of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al., we use the squared roughness
// remapping
float RemapRoughness(float linearRoughness)
{
	return linearRoughness * linearRoughness;
}


// Compute the half-direction vector: The normal of the microsurface that scatters light from an incident direction to
// an outgoing direction
// Based on section 4.1 (p.4) of "Microfacet Models for Refraction through Rough Surfaces", Walter et al., and 
// p.8 of "A Reflectance Model For Computer Graphics", Cook, Torrance.
float3 ComputeH(float3 L, float3 V)
{
	return L + V;
}

float3 ComputeNormalizedH(float3 L, float3 V)
{
	return normalize(ComputeH(L, V));
}


// Specular D is the normal distribution function (NDF), which approximates the surface area of microfacets aligned with
// the halfway vector between the light and view directions.
// As per Disney this is the GGX/Trowbridge-Reitz NDF, with their roughness reparameterization of alpha = roughness^2
float SpecularD(float remappedRoughness, float NoH)
{	
	// Note: Disney reparameterizes alpha = roughness^2. This is our remapping, so we pass it in here
	const float alpha = remappedRoughness; 
	const float alpha2 = alpha * alpha;
	const float NoH2 = NoH * NoH;
	
	return alpha2 / max((M_PI * pow((NoH2 * (alpha2 - 1.f) + 1.f), 2.f)), FLT_MIN); // == 1/pi when roughness = 1
}


// Compute the blended Fresnel reflectance at incident angles (i.e L == N).
// The linearAlbedo defines the diffuse albedo for non-metallic surfaces, and the Fresnel reflectance at normal
// incidence for metallic surfaces. Thus, the linearMetalness value is used to blend between these.
// Based on section B.3.5 "Metal BRDF and Dielectric BRDF" of the glTF 2.0 specifications
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metal-brdf-and-dielectric-brdf
float3 ComputeBlendedF0(float3 f0, float3 linearAlbedo, float linearMetalness)
{
	return lerp(f0, linearAlbedo, linearMetalness);
}


// Compute the F_D90 term (i.e. reflectivity at grazing angles) for the Schlick Fresnel approximation used in our
// Cook-Torrance microfacet specular BRDF. 
//	theta_d = LoH, the cosine of the angle between the light vector, and the micronormal (aka. the half vector).
// Based on equation 5 (p.9) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al; and 
// section 5.3 (p.14) of "Physically Based Shading at Disney", Burley.
float ComputeF90(float remappedRoughness, float LoH)
{
	return 0.5f + 2.f * remappedRoughness * LoH * LoH;
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
	// f90 = saturate(50.0 * dot( f0 , 0.33f) );
	
	return f0 + (f90 - f0) * pow(1.f - u, 5.f);
}


// GGX Microfacet distribution function
float GeometrySchlickGGX(float NoV, float remappedRoughness)
{
	return NoV / ((NoV * (1.f - remappedRoughness)) + remappedRoughness);
}


// Bidirectional geometry shadow-masking function G.
// Describes the fraction of the microsurface normal visible in both the incoming and outgoing direction.
// This is the Smith G, as per section 5.1 of "Microfacet Models for Refraction through Rough Surfaces (Walter et al).
// Bidirectional shadow masking is approximated as the product of 2 monodirectional shadowing terms
float GeometryG(float NoV, float NoL, float remappedRoughness)
{
	const float ggx1 = GeometrySchlickGGX(NoV, remappedRoughness);
	const float ggx2 = GeometrySchlickGGX(NoL, remappedRoughness);
	
	return ggx1 * ggx2;
}


// Note: The original Disney diffuse model is not energy conserving. This implementation from Frostbite is a 
// modification that renormalizes it to make it _almost_ energy conserving
// Based on listing 1 (p.10) "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
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


// Compute the diffuse color. For smooth, shiny metals we blend towards black as the specular contribution increases.
// Based on section B.3.5 "Metal BRDF and Dielectric BRDF" of the glTF 2.0 specifications
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metal-brdf-and-dielectric-brdf
float3 ComputeDiffuseColor(float3 linearAlbedo, float3 f0, float metalness)
{
	return linearAlbedo * (1.f - f0) * (1.f - metalness); // As per the GLTF specs
}


float3 ApplyExposure(float3 linearColor, float exposure)
{
	return linearColor * exposure;
}


struct LightingParams
{
	float3 LinearAlbedo;
	float3 WorldNormal;
	float LinearRoughness;
	float RemappedRoughness;
	float LinearMetalness;
	float3 WorldPosition;
	float3 F0;
	
	float NoL;
	
	float3 LightWorldPos; // 0 for directional lights
	float3 LightWorldDir; // From point to light
	float3 LightColor;
	float LightIntensity;
	float LightAttenuationFactor;
	
	float ShadowFactor;
	
	float3 CameraWorldPos;
	float Exposure;
	
	float DiffuseScale;
	float SpecularScale;
};


float3 ComputeLighting(LightingParams lightingParams)
{
	const float3 N = normalize(lightingParams.WorldNormal);
	
	const float3 V = normalize(lightingParams.CameraWorldPos - lightingParams.WorldPosition); // point -> camera
	const float NoV = saturate(max(dot(N, V), FLT_EPSILON)); // Prevent NaNs at glancing angles

	const float3 L = normalize(lightingParams.LightWorldDir);
	const float NoL = max(lightingParams.NoL, FLT_EPSILON); // Prevent NaNs at glancing angles
	
	const float3 H = ComputeNormalizedH(L, V);
	const float LoH = saturate(dot(L, H));
	
	const float diffuseResponse = FrostbiteDisneyDiffuse(NoV, NoL, LoH, lightingParams.LinearRoughness);
	
	const float3 sunHue = lightingParams.LightColor;
	const float sunIlluminanceLux = lightingParams.LightIntensity;
	
	const float3 illuminance = 
		sunIlluminanceLux * sunHue * NoL * lightingParams.LightAttenuationFactor * lightingParams.ShadowFactor;

	const float3 dielectricSpecular = lightingParams.F0;
	const float3 blendedF0 =
		ComputeBlendedF0(dielectricSpecular, lightingParams.LinearAlbedo, lightingParams.LinearMetalness);
	const float3 diffuseReflectance = ComputeDiffuseColor(
		lightingParams.LinearAlbedo, 
		blendedF0, 
		lightingParams.LinearMetalness) * diffuseResponse * lightingParams.DiffuseScale;
	
	const float f90 = ComputeF90(lightingParams.LinearRoughness, LoH);
	const float3 fresnelF = FresnelSchlickF(blendedF0, f90, LoH);
	
	const float geometryG = GeometryG(NoV, NoL, lightingParams.RemappedRoughness);
	
	const float NoH = saturate(dot(N, H));
	const float specularD = SpecularD(lightingParams.RemappedRoughness, NoH);
	
	const float3 specularReflectance = fresnelF * geometryG * specularD * lightingParams.SpecularScale;
	
	const float3 combinedContribution = (diffuseReflectance + specularReflectance) * illuminance;
	// Note: We're omitting the pi term in the albedo
	
	return combinedContribution;
}


#endif