// © 2023 Adam Badke. All rights reserved.
#include "SaberComputeCommon.hlsli"
#include "SaberGlobals.hlsli"

#define NUM_SAMPLES 1024

RWTexture2D<float2> output0 : register(u0);



// TODO: Move these helper functions somewhere appropriate!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

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
float2 Hammersley2D(uint i, uint N)
{
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}


//  Get a sample vector near a microsurface's halfway vector, from input roughness and a the low-discrepancy sequence value
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	const float a = roughness * roughness;

	const float phi = M_2PI * Xi.x;
	const float cosTheta = sqrt((1.f - Xi.y) / (1.f + (a * a - 1.f) * Xi.y));
	const float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	// Convert spherical -> cartesian coordinates
	const float3 H = float3(
		cos(phi) * sinTheta,
		sin(phi) * sinTheta,
		cosTheta);

	// from tangent-space vector to world-space sample vector
	const float3 up = abs(N.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	const float3 tangent = normalize(cross(up, N));
	const float3 bitangent = cross(N, tangent);

	const float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;

	return normalize(sampleVec);
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
	const uint2 targetResolution = uint2(1024, 1024); // TODO: Pass this in via a parameter block
	
	const uint2 texelCoord = In.DTId.xy;
	
	float2 screenUV = PixelCoordsToUV(texelCoord, targetResolution.xy);
	screenUV.y = 1.f - screenUV.y; // Need to flip Y

	const float NoV = screenUV.x;
	const float NoV2 = NoV * NoV;
	const float roughness = screenUV.y;

	const float3 V = float3(sqrt(1.f - NoV2), 0.f, NoV);

	const float3 N = float3(0.f, 0.f, 1.f);

	float fresnelScale = 0.f;
	float fresnelBias = 0.f;

	for (uint i = 0; i < NUM_SAMPLES; i++)
	{
		const float2 Xi = Hammersley2D(i, NUM_SAMPLES);
		const float3 H = ImportanceSampleGGX(Xi, N, roughness);
		const float3 L = normalize(2.f * dot(V, H) * H - V);

		const float NoL = max(L.z, 0.f);
		const float NoH = max(H.z, 0.f);
		const float VoH = max(dot(V, H), 0.f);

		if (NoL > 0.f)
		{
			const float G = GeometrySmith(NoV, NoL, roughness);
			const float GVis = (G * VoH) / max((NoH * NoV), FLT_MIN); // Shouldn't be zero, but just in case
			const float Fc = pow(1.f - VoH, 5.0f);

			fresnelScale += (1.f - Fc) * GVis;
			fresnelBias += Fc * GVis;
		}
	}

	// Average the results:
	fresnelScale /= NUM_SAMPLES;
	fresnelBias /= NUM_SAMPLES;
	
	output0[texelCoord] = float2(fresnelScale, fresnelBias);
	
	// TODO: Bug here. We're getting some weird artifacts in the final texture
}