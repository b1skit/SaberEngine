// © 2025 Adam Badke. All rights reserved.
#ifndef SE_BINDLESS_RESOURCE_PARAMS
#define SE_BINDLESS_RESOURCE_PARAMS

#include "PlatformConversions.h"


struct VertexStreamInstanceIndices
{
	uint4 g_posNmlTanUV0;				// .xyzw = Position, Normal, Tangent, TexCoord0
	uint4 g_UV1ColBlendIdxBlendWgt;		// .xyzw = TexCoord1, Color, Blend Indices, BlendWeights
	uint4 g_index;						// .x = Indices, .yzw = unused
};


#endif // SE_BINDLESS_RESOURCE_PARAMS