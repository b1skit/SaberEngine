#version 460

#define SABER_VEC4_OUTPUT
#define VOUT_LOCAL_POS
#include "HLSLToGLSL.glsli"

#include "Random.hlsli"
#include "SaberCommon.glsli"
#include "Sampling.hlsli"
#include "UVUtils.glsli"

#include "../Common/IBLGenerationParams.h"


layout(binding=12) uniform IEMPMREMGenerationParams { IEMPMREMGenerationData _IEMPMREMGenerationParams; };

layout(binding=0) uniform sampler2D Tex0;

//#define SAMPLE_HAMMERSLEY
#define SAMPLE_FIBONACCI

// The IEM (Irradiance Environment Map) is the pre-integrated per-light-probe LD term of the diffuse portion of the
// decomposed approximate microfacet BRDF.
// Based on listing 20 (p. 67) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
void PShader()
{
	// We need to build a referential coordinate system with respect to the current world-space sample direction in
	// order to importance sample the hemisphere about it. Here, we choose up vectors for each face that guarantee we 
	// won't try and compute cross(N, N) while constructing the referential basis
	const vec3 upDir[6] = {
		vec3(0.f,		1.f,	0.f),	// X+
		vec3(0.f,		1.f,	0.f),	// X-
		vec3(0.f,		0.f,	-1.f),	// Y+
		vec3(0.f,		0.f,	1.f),	// Y-
		vec3(0.f,		1.f,	0.f),	// Z+
		vec3(0.f,		1.f,	0.f)	// Z-
	};
	
	const uint numSamples = uint(_IEMPMREMGenerationParams.g_numSamplesRoughnessFaceIdx.x);
	const uint faceIdx = uint(_IEMPMREMGenerationParams.g_numSamplesRoughnessFaceIdx.w);
	const float srcMip = _IEMPMREMGenerationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips.x;
	
	// World-space direction from the center of the cube towards the current cubemap pixel
	const vec3 N = normalize(In.LocalPos);
	
#if defined(SAMPLE_FIBONACCI)
	const uint maxDimension = uint(max(
		_IEMPMREMGenerationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips.y,
		_IEMPMREMGenerationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips.z));
	
	RNGState1D rngState = InitializeRNGState1D(uint3(
		(1.f + N.x) * maxDimension,
		(1.f + N.y) * maxDimension,
		(1.f + N.z) * maxDimension));
	
	const float angularOffset = GetNextFloat(rngState);
#endif
	
	const Referential localReferential = BuildReferential(N, upDir[faceIdx]);
	
	vec3 result = vec3(0.f, 0.f, 0.f);
	for (uint i = 0; i < numSamples; i++)
	{		
		vec3 L;
		float NoL;
		float pdf;

#if defined (SAMPLE_HAMMERSLEY)
		const float2 eta = Hammersley2D(i, numSamples);
		ImportanceSampleCosDir(eta, localReferential, L, NoL, pdf);
		
#elif defined(SAMPLE_FIBONACCI)
		ImportanceSampleFibonacciSpiralDir(i, numSamples, angularOffset, localReferential, L, NoL, pdf);
#endif	

		if (NoL > 0)
		{
			const vec2 sphericalUV = WorldDirToSphericalUV(L);

			result += textureLod(Tex0, sphericalUV, srcMip).rgb;
		}
	}
	
	FragColor = vec4(result / numSamples, 1.f);
}