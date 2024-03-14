// © 2024 Adam Badke. All rights reserved.
#ifndef SE_SKYBOX_PARAMS
#define SE_SKYBOX_PARAMS

#include "PlatformConversions.h"


struct SkyboxData
{
	float4 g_backgroundColorIsEnabled; // .rgb = background color override, .a = enabled/disabled (1.f/0.f)

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "SkyboxParams";
#endif
};


#endif // SE_SKYBOX_PARAMS