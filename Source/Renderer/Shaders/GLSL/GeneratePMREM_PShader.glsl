// © 2023 Adam Badke. All rights reserved.
#version 460
#define SABER_VEC4_OUTPUT
#define VOUT_LOCAL_POS

#include "MathConstants.hlsli"
#include "SaberCommon.glsli"
#include "Lighting.glsli"
#include "UVUtils.glsli"

#include "HLSLToGLSL.glsli"
#include "Sampling.hlsli"

#include "../Common/IBLGenerationParams.h"


layout(binding=12) uniform IEMPMREMGenerationParams { IEMPMREMGenerationData _IEMPMREMGenerationParams; };

layout(binding=0) uniform sampler2D Tex0;


// The PMREM (Pre-filtered Mip-mapped Radiance Environment Map) is the pre-integrated per-light-probe LD term of the 
// specular portion of the decomposed approximate microfacet BRDF.
// Based on listing 19 (p. 66) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
void PShader()
{
	// We need to build a referential coordinate system with respect to the current world-space sample direction in
	// order to importance sample the hemisphere about it. Here, we choose up vectors for each face that guarantee we 
	// won't try and compute cross(N, N) while constructing the referential basis
	const vec3 upDir[6] =
	{
		vec3(0.f, 1.f, 0.f), // X+
		vec3(0.f, 1.f, 0.f), // X-
		vec3(0.f, 0.f, -1.f), // Y+
		vec3(0.f, 0.f, 1.f), // Y-
		vec3(0.f, 1.f, 0.f), // Z+
		vec3(0.f, 1.f, 0.f) // Z-
	};
	
	const uint numSamples = uint(_IEMPMREMGenerationParams.g_numSamplesRoughnessFaceIdx.y);
	const float linearRoughness = _IEMPMREMGenerationParams.g_numSamplesRoughnessFaceIdx.z;
	float remappedRoughness = RemapRoughness(linearRoughness);
	const uint faceIdx = uint(_IEMPMREMGenerationParams.g_numSamplesRoughnessFaceIdx.w);
	
	const float srcWidth = _IEMPMREMGenerationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips.y;
	const float srcHeight = _IEMPMREMGenerationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips.z;
	const float numSrcMips = _IEMPMREMGenerationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips.w;
	
	// World-space direction from the center of the cube towards the current cubemap pixel
	const vec3 N = normalize(In.LocalPos);
	
	const vec3 V = N; // We pre-integrate the result for the normal direction N == V
	
	vec3 result = vec3(0.f, 0.f, 0.f);
	vec3 resultWeight = vec3(0.f, 0.f, 0.f);
	
	for (uint i = 0; i < numSamples; i++)
	{
		const vec2 eta = Hammersley2D(i, numSamples);
		
		const Referential localReferential = BuildReferential(N, upDir[faceIdx]);
		
		vec3 H = ImportanceSampleGGXDir(eta, linearRoughness, localReferential); 
		const vec3 L = normalize(2.f * dot(V, H) * H - V);
		
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
			
			const float NoH = clamp(dot(N, H), 0.f, 1.f);
			const float LoH = clamp(dot(L, H), 0.f, 1.f);

			// We get undefined results (pdf = 0 and/or sampling artifacts) when roughness ~= 0. 
			const float k_minRoughness = 0.004f; // Empirically-chosen
			
			const float finalRoughness = max(remappedRoughness, k_minRoughness);
			
			const float pdf = (SpecularD(finalRoughness, NoH) / M_PI) * (NoH / max((4.f * LoH), FLT_MIN));
			
			const float omegaS = 1.f / (numSamples * max(pdf, FLT_MIN));
			const float omegaP = M_4PI / (6.f * srcWidth * srcHeight);
			const float mipLevel = clamp(0.5f * log2(omegaS / omegaP), 0.f, numSrcMips);

			const vec2 sphericalUV = WorldDirToSphericalUV(L);
			
			const vec3 Li = texture(Tex0, sphericalUV, mipLevel).rgb;

			result += Li * NoL;
			resultWeight += NoL;
		}
	}	
	
	FragColor = vec4(result / max(resultWeight, vec3(FLT_MIN,FLT_MIN,FLT_MIN)), 1.0);
}