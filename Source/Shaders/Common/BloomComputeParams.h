// © 2024 Adam Badke. All rights reserved.
#ifndef SE_BLOOM_COMPUTE_PARAMS
#define SE_BLOOM_COMPUTE_PARAMS

#include "PlatformConversions.h"


struct BloomComputeData
{
	float4 g_srcTexDimensions;
	float4 g_dstTexDimensions;
	float4 g_srcMipDstMipFirstUpsampleSrcMipIsDownStage; // .xy = src/dst mip, .z = 1st upsample src mip, .w = isDownStage
	float4 g_bloomRadiusWidthHeightLevelNumLevls; // .xy = bloom width/height, .z = level .w = current level
	float4 g_bloomDebug; // .x = Deflicker enabled

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "BloomComputeParams";
#endif
};


#endif // SE_BLOOM_COMPUTE_PARAMS