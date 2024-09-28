// © 2024 Adam Badke. All rights reserved.
#ifndef SE_ANIMATION_PARAMS
#define SE_ANIMATION_PARAMS

#include "PlatformConversions.h"

// GLTF Specs: The number of morph targets is not limited; A minimum of 8 morphed attributes must be supported
#define NUM_MORPH_TARGETS 8

#if defined(__cplusplus)
static constexpr char const* const s_interleavedMorphDataShaderName = "MorphData";
#endif


struct AnimationData
{
	float4 g_morphWeights[NUM_MORPH_TARGETS / 4];

	/* 
	TODO: Access this with a getter like so:
	const uint baseIdx = weightIdx / 4;
	const uint offsetIdx = weightIdx % 4;
	return g_morphWeights[baseIdx][offsetIdx];
	*/

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "AnimationParams";
	static constexpr uint8_t k_numMorphTargets = NUM_MORPH_TARGETS;
#endif
};


#endif // SE_ANIMATION_PARAMS