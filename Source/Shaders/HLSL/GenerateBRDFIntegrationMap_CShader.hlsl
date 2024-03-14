// © 2023 Adam Badke. All rights reserved.
#include "Lighting.hlsli"
#include "MathConstants.hlsli"
#include "SaberComputeCommon.hlsli"
#include "Color.hlsli"
#include "Sampling.hlsli"
#include "UVUtils.hlsli"

#include "../Common/IBLGenerationParams.h"


ConstantBuffer<BRDFIntegrationParamsData> BRDFIntegrationParams;


#define NUM_SAMPLES 1024


[numthreads(1, 1, 1)]
void CShader(ComputeIn In)
{
	// As per Karis, Microfacet BRDFs can be approximated by decomposing them into the product of 2 terms: LD, and DFG.
	// These terms can be independently pre-computed. LD must be precomputed per light probe, DFG can be precomputed
	// once and reused for all light probes.
	// Here we precompute the DFG term as a 2D LUT, indexed by x = NoV (the view angle w.r.t a surface), and 
	// y = linearRoughness
	
	const uint2 targetResolution = BRDFIntegrationParams.g_integrationTargetResolution.xy;
	
	const uint2 texelCoord = In.DTId.xy;
	
	float2 screenUV = PixelCoordsToUV(texelCoord, targetResolution.xy, float2(0.5f, 0.5f));
	
	const float NoV = screenUV.x;
	const float NoV2 = NoV * NoV;
	
	const float linearRoughness = screenUV.y;
	const float remappedRoughness = RemapRoughness(linearRoughness);

	
	// When screenUV.x = 0, V = (1, 0, 0). When screenUV.x = 1, V = (0, 0, 1)
	const float3 V = float3(
		sqrt(1.f - NoV2), // sin
		0.f,
		NoV); // cos

	const float3 N = float3(0.f, 0.f, 1.f); // Satisfies NoV, given V above

	float4 result = 0;

	for (uint i = 0; i < NUM_SAMPLES; i++)
	{
		const float2 Xi = Hammersley2D(i, NUM_SAMPLES); // eta: A random sample
		const float3 H = ImportanceSampleGGXDir(N, Xi, linearRoughness); // Importance-sampled halfway vector
		float3 L = 2.f * dot(V, H) * H - V; // Light vector: Reflect about the halfway vector, & reverse direction

		float NoL = saturate(L.z);
		const float LoH = saturate(dot(L, H));
		
		const float G = GeometryG(NoV, NoL, remappedRoughness);

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
		ImportanceSampleCosDir(N, XiFrac, L, NoL, pdf);
		if (NoL > 0.f)
		{
			const float NoV = saturate(dot(N, V));

			result.z += FrostbiteDisneyDiffuse(NoV, NoL, LoH, linearRoughness);
		}
	}

	// Average the results:
	output0[texelCoord] = result / NUM_SAMPLES;
}