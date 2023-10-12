// � 2023 Adam Badke. All rights reserved.
#define VOUT_LOCAL_POS

#include "SaberCommon.hlsli"
#include "Sampling.hlsli"
#include "UVUtils.hlsli"


// The IEM (Irradiance Environment Map) is the pre-integrated per-light-probe LD term of the diffuse portion of the
// decomposed approximate microfacet BRDF.
// Based on listing 20 (p. 67) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
float4 PShader(VertexOut In) : SV_Target
{
	// We need to build a referential coordinate system with respect to the current world-space sample direction in
	// order to importance sample the hemisphere about it. Here, we choose up vectors for each face that guarantee we 
	// won't try and compute cross(N, N) while constructing the referential basis
	static const float3 upDir[6] = {
		float3(0.f,		1.f,	0.f),	// X+
		float3(0.f,		1.f,	0.f),	// X-
		float3(0.f,		0.f,	-1.f),	// Y+
		float3(0.f,		0.f,	1.f),	// Y-
		float3(0.f,		1.f,	0.f),	// Z+
		float3(0.f,		1.f,	0.f)	// Z-
	};
	
	static const uint numSamples = IEMPMREMGenerationParams.g_numSamplesRoughnessFaceIdx.x;
	static const uint faceIdx = IEMPMREMGenerationParams.g_numSamplesRoughnessFaceIdx.w;
	static const float srcMip = IEMPMREMGenerationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips.x;
	
	// World-space direction from the center of the cube towards the current cubemap pixel
	const float3 N = normalize(In.LocalPos);
	
	float3 result = float3(0.f, 0.f, 0.f);
	
	for (uint i = 0; i < numSamples; i++)
	{
		const float2 eta = Hammersley2D(i, numSamples);

		const Referential localReferential = BuildReferential(N, upDir[faceIdx]);
		
		float3 L;
		float NoL;
		float pdf;
		ImportanceSampleCosDir(eta, localReferential, L, NoL, pdf);

		if (NoL > 0)
		{
			const float2 sphericalUV = WorldDirToSphericalUV(L);
			
			result += Tex0.SampleLevel(Wrap_LinearMipMapLinear_Linear, sphericalUV, srcMip).rgb;
		}
	}
	
	return float4(result / numSamples, 1.f);
}