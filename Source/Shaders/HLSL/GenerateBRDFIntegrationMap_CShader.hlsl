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


// Get a sample vector near a microsurface's halfway vector, from input roughness and a low-discrepancy sequence value
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

	// from tangent-space vector to world-space sample vector
	const float3 up = abs(N.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	const float3 tangent = normalize(cross(up, N));
	const float3 bitangent = cross(N, tangent);

	const float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;

	return sampleVec;
}


// Helper function for geometry function
float GeometrySchlickGGX(float NoV, float remappedRoughness)
{
	const float nom = NoV;
	const float denom = (NoV * (1.f - remappedRoughness)) + remappedRoughness;
	
	return nom / denom;
}


// Geometry function: Compute the proportion of microfacets visible
float GeometrySmith(float NoV, float NoL, float remappedRoughness)
{
	const float ggx1 = GeometrySchlickGGX(NoV, remappedRoughness);
	const float ggx2 = GeometrySchlickGGX(NoL, remappedRoughness);
	
	return ggx1 * ggx2;
}


[numthreads(1, 1, 1)]
void main(ComputeIn In)
{
	// As per Karis, Microfacet BRDFs can be approximated by decomposing them into the product of 2 terms: LD, and DFG.
	// These terms can be independently pre-computed.
	// Here we precompute the DFG term as a 2D LUT, indexed by the view angle w.r.t a surface, and the surface roughness
	
	// TODO: Switch this to the DFG precomputation as per the Frostbite paper
	
	const uint2 targetResolution = BRDFIntegrationParams.g_integrationTargetResolution.xy;
	
	const uint2 texelCoord = In.DTId.xy;
	
	float2 screenUV = PixelCoordsToUV(texelCoord, targetResolution.xy, float2(0.5f, 0.5f));
	screenUV.y = 1.f - screenUV.y; // Need to flip Y
	
	const float NoV = screenUV.x;
	
	const float NoV2 = NoV * NoV;
	const float roughness = screenUV.y;
	
	const float3 V = float3(
		sqrt(1.f - NoV2), // sin
		0.f,
		NoV); // cos

	const float3 N = float3(0.f, 0.f, 1.f);

	float fresnelScale = 0.f; // A
	float fresnelBias = 0.f; // B

	for (uint i = 0; i < NUM_SAMPLES; i++)
	{
		const float2 Xi = Hammersley2D(i, NUM_SAMPLES); // eta: A random sample
		const float3 H = ImportanceSampleGGX(Xi, N, roughness);	// Importance-sampled halfway vector
		const float3 L = 2.f * dot(V, H) * H - V; // Light vector: Reflect about the halfway vector, & reverse direction

		const float NoL = saturate(L.z);
		if (NoL > 0.f)
		{
			const float NoH = saturate(H.z);
			const float VoH = saturate(dot(V, H));
			
			const float G = GeometrySmith(NoV, NoL, roughness);
			const float GVis = (G * VoH) / max((NoH * NoV), FLT_MIN); // Shouldn't be zero, but just in case
			const float Fc = pow(1.f - VoH, 5.f);

			fresnelScale += (1.f - Fc) * GVis;
			fresnelBias += Fc * GVis;
		}
	}

	// Average the results:
	output0[texelCoord] = float4(fresnelScale / NUM_SAMPLES, fresnelBias / NUM_SAMPLES, 0.f, 0.f);
}