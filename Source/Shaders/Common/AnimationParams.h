// © 2024 Adam Badke. All rights reserved.
#ifndef SE_ANIMATION_PARAMS
#define SE_ANIMATION_PARAMS

#include "PlatformConversions.h"

// OpenGL supports a max of 16 SSBOs in a compute shader, we issue additional dispatches to handle more streams
#define MAX_STREAMS_PER_DISPATCH 7

// Compute shader numthreads: We process our vertex attributes in 1D
#define VERTEX_ANIM_THREADS_X 32


struct MorphMetadata
{
	// .x = No. vertices per stream, .y = max morph targets per stream, .z = interleaved morph float stride, .w = unused
	uint4 g_meshPrimMetadata; 

	// .x = vertex float stride, .y = no. components, .zw = unused
	uint4 g_streamMetadata[MAX_STREAMS_PER_DISPATCH];

	// .x = first float offset, .y = float stride (of 1 displacement), .z = no. components, .w = unused
	uint4 g_morphMetadata[MAX_STREAMS_PER_DISPATCH];

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "MorphMetadataParams";
#endif
};


struct MorphDispatchMetadata
{
	uint4 g_dispatchMetadata; // .x = num active buffers, .yzw = unused

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "MorphDispatchMetadataParams";
#endif
};


struct SkinningData
{
	// .x = No. vertices per stream, .yzw = unused
	uint4 g_meshPrimMetadata;


#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "SkinningParams";
#endif
};


#endif // SE_ANIMATION_PARAMS