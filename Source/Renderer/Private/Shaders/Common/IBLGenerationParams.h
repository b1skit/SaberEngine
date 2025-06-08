// © 2024 Adam Badke. All rights reserved.
#ifndef SE_IBL_GENERATION_PARAMS
#define SE_IBL_GENERATION_PARAMS

#include "Private/PlatformConversions.h"


#define BRDF_INTEGRATION_DISPATCH_XY_DIMS 8

struct BRDFIntegrationData
{
	uint4 g_integrationTargetResolution;

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "BRDFIntegrationParams";
#endif
};


struct IEMPMREMGenerationData
{
	float4 g_numSamplesRoughnessFaceIdx; // .x = numIEMSamples, .y = numPMREMSamples, .z = roughness, .w = faceIdx
	float4 g_mipLevelSrcWidthSrcHeightSrcNumMips; // .x = IEM mip level, .yz = src width/height, .w = src num mips

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "IEMPMREMGenerationParams";
#endif
};


#endif // SE_IBL_GENERATION_PARAMS