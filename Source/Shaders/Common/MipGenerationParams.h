// © 2024 Adam Badke. All rights reserved.
#ifndef SE_MIP_GENERATION_PARAMS
#define SE_MIP_GENERATION_PARAMS

#include "PlatformConversions.h"


struct MipGenerationData
{
	float4 g_output0Dimensions; // .xyzw = width, height, 1/width, 1/height of the output0 texture
	uint4 g_mipParams; // .xyzw = srcMipLevel, numMips, srcDimensionMode, faceIdx
	float4 g_isSRGB; // .x = isSRGB, .yzw = unused

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "MipGenerationParams";
#endif
};


#endif // SE_MIP_GENERATION_PARAMS