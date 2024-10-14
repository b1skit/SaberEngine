// © 2024 Adam Badke. All rights reserved.
#ifndef SE_ANIMATION_PARAMS
#define SE_ANIMATION_PARAMS

#include "PlatformConversions.h"

// Equivalent to gr::VertexStream::k_maxVertexStreams
#define NUM_VERTEX_STREAMS 16

// Compute shader numthreads: We process our vertex attributes in 1D
#define VERTEX_ANIM_THREADS_X 16

// GLTF Specs: The number of morph targets is not limited; A minimum of 8 morphed attributes must be supported.
#define NUM_MORPH_TARGETS 16

#if defined(__cplusplus)
static constexpr char const* const s_interleavedMorphDataShaderName = "MorphData";
#endif


struct VertexStreamMetadata
{
	uint4 g_meshPrimMetadata; // .x = No. vertices per stream, .yzw = unused

	// Each element of this array corresponds with a vertex stream slot on our MeshPrimitive
	// .x = Float stride (i.e. No. floats per element: float3 = 3). 0 = No buffer bound (i.e. end of valid/bound buffers)
	// .y = unused
	// .z = unused
	// .w = unused
	uint4 g_perStreamMetadata[NUM_VERTEX_STREAMS]; 
	

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "VertexStreamParams";
#endif
};


struct AnimationData
{
	// Morph targets 
	// Shader-side version of gr::MeshPrimitive::MeshRenderData
	// Must be reinterpreted as an array of floats with AnimationData::k_numMorphTargets elements:
	// i.e.		g_morphWeights[targetIdx / 4][targetIdx % 4]
	float4 g_morphWeights[NUM_MORPH_TARGETS / 4];

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "AnimationParams";
	static constexpr uint8_t k_numMorphTargets = NUM_MORPH_TARGETS;
#endif
};


#endif // SE_ANIMATION_PARAMS