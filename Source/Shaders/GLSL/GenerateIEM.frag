#version 460

#define SABER_VEC4_OUTPUT
#define VOUT_LOCAL_POS

#include "MathConstants.glsl"
#include "SaberCommon.glsl"
#include "SaberGlobals.glsl"
#include "SaberLighting.glsl"


// The IEM (Irradiance Environment Map) is the pre-integrated per-light-probe LD term of the diffuse portion of the
// decomposed approximate microfacet BRDF.
// Based on listing 20 (p. 67) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
void main()
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
	
	const uint numSamples = uint(g_numSamplesRoughnessFaceIdx.x);
	const uint faceIdx = uint(g_numSamplesRoughnessFaceIdx.w);
	const float srcMip = g_mipLevelSrcWidthSrcHeightSrcNumMips.x;
	
	// World-space direction from the center of the cube towards the current cubemap pixel
	const vec3 N = normalize(vOut.LocalPos);
	
	vec3 result = vec3(0.f, 0.f, 0.f);
	
	for (uint i = 0; i < numSamples; i++)
	{
		const vec2 eta = Hammersley2D(i, numSamples);

		const Referential localReferential = BuildReferential(N, upDir[faceIdx]);
		
		vec3 L;
		float NoL;
		float pdf;
		ImportanceSampleCosDir(eta, localReferential, L, NoL, pdf);

		if (NoL > 0)
		{
			const vec2 sphericalUV = WorldDirToSphericalUV(L);

			result += texture(Tex0, sphericalUV, srcMip).rgb;
		}
	}
	
	FragColor = vec4(result / numSamples, 1.f);
}