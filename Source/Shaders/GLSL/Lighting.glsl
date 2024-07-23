// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_LIGHTING
#define SABER_LIGHTING

#include "MathConstants.glsl"
#include "SaberCommon.glsl"


// As per Cem Yuksel's nonsingular point light attenuation function:
// http://www.cemyuksel.com/research/pointlightattenuation/
float ComputeNonSingularAttenuationFactor(vec3 worldPos, vec3 lightPos, float emitterRadius)
{
	const float r2 = emitterRadius * emitterRadius;

	const float lightDistance = length(worldPos - lightPos);
	const float d2 = lightDistance * lightDistance;
	
	const float attenuation = 2.f / (d2 + r2 + (lightDistance * sqrt(d2 + r2)));
	
	return attenuation;
}


// As per equation 64, section 5.2.2.2 of "Physically Based Rendering in Filament"
// https://google.github.io/filament/Filament.md.html#lighting/directlighting/punctuallights
float GetSpotlightAngleAttenuation(
	vec3 toLight, 
	vec3 lightWorldForwardDir, 
	float innerConeAngle, 
	float outerConeAngle, 
	float cosOuterAngle, 
	float scaleTerm,
	float offsetTerm)
{
	const float cd = dot(normalize(-toLight), lightWorldForwardDir);
	
	float attenuation = clamp(cd * scaleTerm + offsetTerm, 0.f, 1.f);
	
	return attenuation * attenuation; // Smooths the resulting transition
}


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
vec3 ComputeH(vec3 L, vec3 V)
{
	return L + V;
}

vec3 ComputeNormalizedH(vec3 L, vec3 V)
{
	return normalize(ComputeH(L, V));
}


// Helper function for geometry function
float GeometrySchlickGGX(float NoV, float remappedRoughness)
{
	return NoV / ((NoV * (1.f - remappedRoughness)) + remappedRoughness);
}


// Specular G: 
// Geometry function: Compute the proportion of microfacets visible
float GeometryG(float NoV, float NoL, float remappedRoughness)
{
	float ggx1 = GeometrySchlickGGX(NoV, remappedRoughness);
	float ggx2 = GeometrySchlickGGX(NoL, remappedRoughness);
	
	return ggx1 * ggx2;
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
vec3 ComputeBlendedF0(vec3 f0, vec3 linearAlbedo, float linearMetalness)
{
	return mix(f0, linearAlbedo, linearMetalness);
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
vec3 FresnelSchlickF(in vec3 f0, in float f90, in float u)
{
	// Schuler's solution for specular micro-occlusion.
	// derived from f0 (which is itself derived from the diffuse color), based on the knowledge that no real material
	// has a reflectance < 2%. Values of reflectance < 0.02 are assumed to be the result of pre-baked occlusion, and 
	// used to smoothly decrease the Fresnel reflectance contribution
	// f90 = saturate(50.0 * dot( f0 , 0.33f) );
	
	return f0 + (vec3(f90, f90, f90) - f0) * pow(1.f - u, 5.f);
}


vec3 ApplyExposure(vec3 linearColor, float exposure)
{
	return linearColor * exposure;
}


// Note: The original Disney diffuse model is not energy conserving. This implementation from Frostbite is a 
// modification that renormalizes it to make it _almost_ energy conserving
// Based on listing 1 (p.10) "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
float FrostbiteDisneyDiffuse(float NoV, float NoL, float LoH, float linearRoughness)
{
	const float energyBias		= mix(0.f, 0.5f, linearRoughness);
	const float energyFactor	= mix(1.f, 1.f / 1.51f, linearRoughness);
	const float fd90			= energyBias + 2.f * LoH * LoH * linearRoughness;
	const vec3 f0				= vec3(1.f, 1.f, 1.f);
	const float lightScatter	= FresnelSchlickF(f0, fd90, NoL).r;
	const float viewScatter		= FresnelSchlickF(f0, fd90, NoV).r;
	
	return lightScatter * viewScatter * energyFactor;
}


// Compute the diffuse color. For smooth, shiny metals we blend towards black as the specular contribution increases.
// Based on section B.3.5 "Metal BRDF and Dielectric BRDF" of the glTF 2.0 specifications
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metal-brdf-and-dielectric-brdf
vec3 ComputeDiffuseColor(vec3 linearAlbedo, vec3 f0, float metalness)
{
	return linearAlbedo * (vec3(1.f, 1.f, 1.f) - f0) * (1.f - metalness); // As per the GLTF specs
}


struct LightingParams
{
	vec3 LinearAlbedo;
	vec3 WorldNormal;
	float LinearRoughness;
	float RemappedRoughness;
	float LinearMetalness;
	vec3 WorldPosition;
	vec3 F0;

	float NoL;

	vec3 LightWorldPos; // 0 for directional lights
	vec3 LightWorldDir;
	vec3 LightColor;
	float LightIntensity;
	float LightAttenuationFactor;

	float ShadowFactor;
	
	vec3 CameraWorldPos;
	float Exposure;

	float DiffuseScale; 
	float SpecularScale;
};


vec3 ComputeLighting(const LightingParams lightingParams)
{
	const vec3 N = normalize(lightingParams.WorldNormal);
	
	const vec3 V = normalize(lightingParams.CameraWorldPos - lightingParams.WorldPosition); // point -> camera
	const float NoV = clamp(max(dot(N, V), FLT_EPSILON), 0.f, 1.f); // Prevent NaNs at glancing angles

	const vec3 L = normalize(lightingParams.LightWorldDir);
	const float NoL = clamp(max(lightingParams.NoL, FLT_EPSILON), 0.f, 1.f); // Prevent NaNs at glancing angles
	
	const vec3 H = ComputeNormalizedH(L, V);
	const float LoH = clamp(dot(L, H), 0.f, 1.f);
	
	const float diffuseResponse = FrostbiteDisneyDiffuse(NoV, NoL, LoH, lightingParams.LinearRoughness);
	
	const vec3 sunHue = lightingParams.LightColor;
	const float sunIlluminanceLux = lightingParams.LightIntensity;
	
	const vec3 illuminance = 
		sunIlluminanceLux * sunHue * NoL * lightingParams.LightAttenuationFactor * lightingParams.ShadowFactor;

	const vec3 dielectricSpecular = lightingParams.F0;
	const vec3 blendedF0 =
		ComputeBlendedF0(dielectricSpecular, lightingParams.LinearAlbedo, lightingParams.LinearMetalness);
	const vec3 diffuseReflectance = ComputeDiffuseColor(
		lightingParams.LinearAlbedo, 
		blendedF0, 
		lightingParams.LinearMetalness) * diffuseResponse * lightingParams.DiffuseScale;
	
	const float f90 = ComputeF90(lightingParams.LinearRoughness, LoH);
	const vec3 fresnelF = FresnelSchlickF(blendedF0, f90, LoH);
	
	const float geometryG = GeometryG(NoV, NoL, lightingParams.RemappedRoughness);
	
	const float NoH = clamp(dot(N, H), 0.f, 1.f);
	const float specularD = SpecularD(lightingParams.RemappedRoughness, NoH);
	
	const vec3 specularReflectance = fresnelF * geometryG * specularD * lightingParams.SpecularScale;
	
	const vec3 combinedContribution = (diffuseReflectance + specularReflectance) * illuminance;
	// Note: We're omitting the pi term in the albedo
	
	return combinedContribution;
}


// Sampling:
//----------

// Helper function: Compute the Van der Corput sequence via radical inverse
// Based on:  http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html (As per Hacker's Delight)
float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}


// Compute the i'th Hammersley point, of N points
// Based on:  http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
vec2 Hammersley2D(uint i, uint N)
{
	return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}


// A referential RHCS, with N equivalent to Z
struct Referential
{
	vec3 N;				// Equivalent to Z in a RHCS
	vec3 TangentX;		// Equivalent to right/X in a RHCS
	vec3 BitangentY;	// Equivalent to up/Y in an RHCS
};

// Build a referential coordinate system with respect to a normal vector
Referential BuildReferential(vec3 N, vec3 up)
{
	Referential referential;
	referential.N = N;
	
	referential.TangentX = normalize(cross(up, N));
	referential.BitangentY = cross(N, referential.TangentX);
	
	return referential;
}


// Constructs a best-guess up vector
Referential BuildReferential(vec3 N)
{
	const vec3 up = abs(N.z) < 0.999f ? vec3(0, 0, 1) : vec3(1, 0, 0);
	return BuildReferential(N, up);
}

// Compute a cosine-weighted sample direction
// Based on listing A.2 (p.106) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
void ImportanceSampleCosDir(
	in vec2 u,
	in Referential localReferential,
	out vec3 L,
	out float NoL,
	out float pdf)
{
	const float u1 = u.x;
	const float u2 = u.y;

	const float r = sqrt(u1);

	const float phi = u2 * M_2PI;

	L = vec3(
		r * cos(phi),
		r * sin(phi),
		sqrt(max(0.0f, 1.f - u1)));
	
	L = normalize((localReferential.TangentX * L.y) + (localReferential.BitangentY * L.x) + (localReferential.N * L.z));

	NoL = dot(L, localReferential.N);

	pdf = NoL * M_1_PI;
}


void ImportanceSampleCosDir(
	in vec3 N,
	in vec2 u,
	out vec3 L,
	out float NoL,
	out float pdf)
{	
	Referential localReferential = BuildReferential(N);
	ImportanceSampleCosDir(u, localReferential, L, NoL, pdf);
}


// Get a sample vector near a microsurface's halfway vector, from input roughness and a low-discrepancy sequence value.
// As per Karis, "Real Shading in Unreal Engine 4" (p.4)
vec3 ImportanceSampleGGXDir(in vec2 u, in float roughness, in Referential localReferential)
{
	const float a = roughness * roughness;
	
	const float phi = M_2PI * u.x;
	const float cosTheta = sqrt((1.f - u.y) / (1.f + (a * a - 1.f) * u.y));
	const float sinTheta = sqrt(max(1.f - cosTheta * cosTheta, 0.f));
	
	// Spherical to cartesian coordinates:
	vec3 H = vec3(
		sinTheta * cos(phi),
		sinTheta * sin(phi),
		cosTheta);
	
	// Tangent-space to world-space:
	H = normalize(
		localReferential.TangentX * H.x + 
		localReferential.BitangentY * H.y + 
		localReferential.N * H.z);
	
	return H;
}


vec3 ImportanceSampleGGXDir(in vec3 N, in vec2 u, in float roughness)
{
	Referential localReferential = BuildReferential(N);
	return ImportanceSampleGGXDir(u, roughness, localReferential);
}


#endif // SABER_LIGHTING