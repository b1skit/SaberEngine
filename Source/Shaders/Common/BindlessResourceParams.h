// © 2025 Adam Badke. All rights reserved.
#ifndef SE_BINDLESS_RESOURCE_PARAMS
#define SE_BINDLESS_RESOURCE_PARAMS

#include "PlatformConversions.h"


struct BindlessLUTData
{
	uint4 g_posNmlTanUV0;	// .xyzw = Position, Normal, Tangent, TexCoord0
	uint4 g_UV1ColorIndex;	// .xyzw = TexCoord1, Color, 16 bit indexes, 32 bit indexes

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "BindlessLUT";
#endif
};


#endif // SE_BINDLESS_RESOURCE_PARAMS