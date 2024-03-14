// © 2024 Adam Badke. All rights reserved.
#ifndef SE_IBL_GENERATION_PARAMS
#define SE_IBL_GENERATION_PARAMS

#include "PlatformConversions.h"


struct BRDFIntegrationParamsData
{
	uint4 g_integrationTargetResolution;

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "BRDFIntegrationParams";
#endif
};


struct IEMPMREMGenerationParamsData
{
	float4 g_numSamplesRoughnessFaceIdx; // .x = numIEMSamples, .y = numPMREMSamples, .z = roughness, .w = faceIdx
	float4 g_mipLevelSrcWidthSrcHeightSrcNumMips; // .x = IEM mip level, .yz = src width/height, .w = src num mips

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "IEMPMREMGenerationParams";
#endif
};


#endif // SE_IBL_GENERATION_PARAMS