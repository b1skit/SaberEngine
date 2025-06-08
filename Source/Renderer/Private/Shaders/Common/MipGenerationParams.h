// © 2024 Adam Badke. All rights reserved.
#ifndef SE_MIP_GENERATION_PARAMS
#define SE_MIP_GENERATION_PARAMS

#include "Private/PlatformConversions.h"


struct MipGenerationData
{
	float4 g_output0Dimensions; // .xyzw = width, height, 1/width, 1/height of the output0 texture
	uint4 g_mipParams; // .x = srcMipLevel, .y = numMips, .z = array size/depth, w = unused
	float4 g_resourceParams; // .x = isSRGB, .y = srcDimensionMode, .z = faceIdx, .w = arrayIdx

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "MipGenerationParams";
#endif
};


#endif // SE_MIP_GENERATION_PARAMS