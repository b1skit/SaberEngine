// © 2023 Adam Badke. All rights reserved.
#define VOUT_LOCAL_POS

#include "Lighting.hlsli"
#include "MathConstants.hlsli"
#include "SaberCommon.hlsli"
#include "Sampling.hlsli"
#include "UVUtils.hlsli"


// The PMREM (Pre-filtered Mip-mapped Radiance Environment Map) is the pre-integrated per-light-probe LD term of the 
// specular portion of the decomposed approximate microfacet BRDF.
// Based on listing 19 (p. 66) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
float4 PShader(VertexOut In) : SV_Target
{
	// We need to build a referential coordinate system with respect to the current world-space sample direction in
	// order to importance sample the hemisphere about it. Here, we choose up vectors for each face that guarantee we 
	// won't try and compute cross(N, N) while constructing the referential basis
	static const float3 upDir[6] =
	{
		float3(0.f, 1.f, 0.f), // X+
		float3(0.f, 1.f, 0.f), // X-
		float3(0.f, 0.f, -1.f), // Y+
		float3(0.f, 0.f, 1.f), // Y-
		float3(0.f, 1.f, 0.f), // Z+
		float3(0.f, 1.f, 0.f) // Z-
	};
	
	static const uint numSamples = IEMPMREMGenerationParams.g_numSamplesRoughnessFaceIdx.y;
	static const float roughness = IEMPMREMGenerationParams.g_numSamplesRoughnessFaceIdx.z;
	static const uint faceIdx = IEMPMREMGenerationParams.g_numSamplesRoughnessFaceIdx.w;
	
	static const float srcWidth = IEMPMREMGenerationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips.y;
	static const float srcHeight = IEMPMREMGenerationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips.z;
	static const float numSrcMips = IEMPMREMGenerationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips.w;
	
	// World-space direction from the center of the cube towards the current cubemap pixel
	const float3 N = normalize(In.LocalPos);
	
	const float3 V = N; // We pre-integrate the result for the normal direction N == V
	
	float3 result = float3(0.f, 0.f, 0.f);
	float3 resultWeight = 0.f;
	
	for (uint i = 0; i < numSamples; i++)
	{
		const float2 eta = Hammersley2D(i, numSamples);
		
		const Referential localReferential = BuildReferential(N, upDir[faceIdx]);
		
		float3 H = ImportanceSampleGGXDir(eta, roughness, localReferential);
		const float3 L = normalize(2.f * dot(V, H) * H - V);
		
		const float NoL = dot(N, L);
		
		if (NoL > 0.f)
		{
			// We use pre-filtered importance sampling (i.e. sampling lower mips for samples with low probabilities in
			// order to reduce variance (as per GPU Gems 3, ch.20, "GPU-Based Importance Sampling")
			// https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-20-gpu-based-importance-sampling
			//
			// We're pre-integrating the results here for the normal direction (i.e. N == V). Thus, NoL == LoH.
			// This allows the BRDF pdf to be simplified from:
			//		pdf = D_GGX_Divide_Pi(NoH, roughness) * NoH / (4 * LoH);
			// to
			//		pdf = D_GGX_Divide_Pi(NoH, roughness) / 4;
			//
			// We clamp the mip level to something lower than 8x8 to avoid cubemap filtering issues.
			//
			//	- OmegaS: Solid angle associated with a sample
			//	- OmegaP: Solid angle associated with a pixel of the cubemap
			
			const float NoH = saturate(dot(N, H));
			const float LoH = saturate(dot(L, H));

			// We get undefined results (pdf = 0 and/or sampling artifacts) when roughness ~= 0. 
			static const float minRoughness = 0.004f; // Empirically-chosen
			
			const float finalRoughness = max(roughness, minRoughness);
			
			const float pdf = (SpecularD(finalRoughness, NoH) / M_PI) * (NoH / max((4.f * LoH), FLT_MIN));
			
			const float omegaS = 1.f / (numSamples * max(pdf, FLT_MIN));
			const float omegaP = M_4PI / (6.f * srcWidth * srcHeight);
			const float mipLevel = clamp(0.5f * log2(omegaS / omegaP), 0.f, numSrcMips);

			const float2 sphericalUV = WorldDirToSphericalUV(L);
			
			const float3 Li = Tex0.SampleLevel(Wrap_LinearMipMapLinear_Linear, sphericalUV, mipLevel).rgb;

			result += Li * NoL;
			resultWeight += NoL;
		}
	}	
	
	return float4(result / max(resultWeight, FLT_MIN), 1.f);
}