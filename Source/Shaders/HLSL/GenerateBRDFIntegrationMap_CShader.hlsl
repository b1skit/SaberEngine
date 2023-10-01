// © 2023 Adam Badke. All rights reserved.
#include "MathConstants.hlsli"
#include "SaberComputeCommon.hlsli"
#include "SaberGlobals.hlsli"
#include "Sampling.hlsli"
#include "UVUtils.hlsli"


RWTexture2D<float4> output0 : register(u0);


struct BRDFIntegrationParamsCB
{
	uint4 g_integrationTargetResolution;
};
ConstantBuffer<BRDFIntegrationParamsCB> BRDFIntegrationParams;


#define NUM_SAMPLES 1024


// Get a sample vector near a microsurface's halfway vector, from input roughness and a low-discrepancy sequence value.
// As per Karis, "Real Shading in Unreal Engine 4" (p.4)
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	const float a = roughness * roughness;

	const float phi = M_2PI * Xi.x;

	// Need to guard against divide-by-0 or else we get NaNs
	const float cosTheta = sqrt((1.f - Xi.y) / max(1.f + (a * a - 1.f) * Xi.y, FLT_MIN) );
	const float sinTheta = sqrt(max(1.f - cosTheta * cosTheta, 0.f));
	
	// Convert spherical -> cartesian coordinates
	const float3 H = float3(
		sinTheta * cos(phi),
		sinTheta * sin(phi),
		cosTheta);

	const Referential localReferential = BuildReferential(N);
	
	const float3 sampleVec =
		localReferential.TangentX * H.x + 
		localReferential.BitangentY * H.y + 
		localReferential.N * H.z;

	return sampleVec;
}


// GGX Microfacet distribution function
float GeometrySchlickGGX(float NoV, float roughness)
{
	return NoV / ((NoV * (1.f - roughness)) + roughness);
}


// Bidirectional shadow-masking function G.
// Describes the fraction of the microsurface normal visible in both the incoming and outgoing direction.
// This is the Smith G, as per section 5.1 of "Microfacet Models for Refraction through Rough Surfaces (Walter et al).
// Bidirectional shadow masking is approximated as the product of 2 monodirectional shadowing terms
float Geometry(float NoV, float NoL, float roughness)
{
	const float ggx1 = GeometrySchlickGGX(NoV, roughness);
	const float ggx2 = GeometrySchlickGGX(NoL, roughness);
	
	return ggx1 * ggx2;
}

// Fresnel function F.
// Describes the amount of light reflected from a (smooth) surface at the interface between 2 media.
// This is the Schlick approximation:
// f0 = Reflectance at normal incidence. 
//	f0 = (n_1 - n_2)^2 / (n_1 + n_2)^2, with n_i = the material's index of refraction (IOR). 
//	When one media is air, which has an IOR ~= 1, f0 = (n-1)^2 / (n+1)^2
// f90 = Maximum reflectance (i.e. at grazing incidence, when the normal and ray are 90 degrees apart)
// u = cosine of the angle between the surface normal N and the incident ray
float3 Fresnel(in float3 f0, in float f90, in float u)
{
	// Schuler's solution for specular micro-occlusion.
	// derived from f0 (which is itself derived from the diffuse color), based on the knowledge that no real material
	// has a reflectance < 2%. Values of reflectance < 0.02 are assumed to be the result of pre-baked occlusion, and 
	// used to smoothly decrease the Fresnel reflectance contribution
	// f90 = saturate(50.0 * dot( fresnel0 , 0.33) );
	
	return f0 + (f90 - f0) * pow(1.f - u, 5.f);
}


float Fr_DisneyDiffuse(float NoV, float NoL, float LoH, float linearRoughness)
{
	const float energyBias		= lerp(0.f, 0.5f, linearRoughness);
	const float energyFactor	= lerp(1.f, 1.f / 1.51f, linearRoughness);
	const float fd90			= energyBias + 2.f * LoH * LoH * linearRoughness;
	const float3 f0				= float3(1.f, 1.f, 1.f);
	const float lightScatter	= Fresnel(f0, fd90, NoL).r;
	const float viewScatter		= Fresnel(f0, fd90, NoV).r;
	
	return lightScatter * viewScatter * energyFactor;
}



[numthreads(1, 1, 1)]
void main(ComputeIn In)
{
	// As per Karis, Microfacet BRDFs can be approximated by decomposing them into the product of 2 terms: LD, and DFG.
	// These terms can be independently pre-computed. LD must be precomputed per light probe, DFG can be precomputed
	// once and reused for all light probes.
	// Here we precompute the DFG term as a 2D LUT, indexed by x = NoV (the view angle w.r.t a surface), and 
	// y = surface roughness
	
	const uint2 targetResolution = BRDFIntegrationParams.g_integrationTargetResolution.xy;
	
	const uint2 texelCoord = In.DTId.xy;
	
	float2 screenUV = PixelCoordsToUV(texelCoord, targetResolution.xy, float2(0.5f, 0.5f));
	screenUV.y = 1.f - screenUV.y; // Need to flip Y
	
	const float NoV = screenUV.x;
	
	const float NoV2 = NoV * NoV;
	const float roughness = screenUV.y;
	
	// When screenUV.x = 0, V = (1, 0, 0). When screenUV.x = 1, V = (0, 0, 1)
	const float3 V = float3(
		sqrt(1.f - NoV2), // sin
		0.f,
		NoV); // cos

	const float3 N = float3(0.f, 0.f, 1.f); // Satisfies NoV, given V above

	float4 result = 0; // .x = ?, .y = ?, .z = ?, .w = unused

	for (uint i = 0; i < NUM_SAMPLES; i++)
	{
		const float2 Xi = Hammersley2D(i, NUM_SAMPLES); // eta: A random sample
		const float3 H = ImportanceSampleGGX(Xi, N, roughness); // Importance-sampled halfway vector
		float3 L = 2.f * dot(V, H) * H - V; // Light vector: Reflect about the halfway vector, & reverse direction

		float NoL = saturate(L.z);
		const float LoH = saturate(dot(L, H));
		
		const float G = Geometry(NoV, NoL, roughness);

		if (NoL > 0.f && G > 0.f)
		{
			const float NoH = saturate(H.z);
			
			const float GVis = (G * LoH) / max((NoH * NoV), FLT_MIN); // Shouldn't be zero, but just in case
			
			const float Fc = pow(1.f - LoH, 5.f);

			result.x += (1.f - Fc) * GVis;
			result.y += Fc * GVis;
		}
		
		// Disney diffuse pre-integration:
		const float2 XiFrac = frac(Xi + 0.5f);
		float pdf; // Not used, as it cancels with other terms: 1/PI from the diffuse BRDF, & the NoL from Lambert's law
		ImportanceSampleCosDir(XiFrac, N, L, NoL, pdf);
		if (NoL > 0.f)
		{
			const float NoV = saturate(dot(N, V));

			result.z += Fr_DisneyDiffuse(NoV, NoL, LoH, sqrt(roughness)); // TODO: Why the sqrt(roughness)?
		}
		
	}

	// Average the results:
	output0[texelCoord] = result / NUM_SAMPLES;
}