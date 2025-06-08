// © 2025 Adam Badke. All rights reserved.
#ifndef SE_TRANSFORM_PARAMS
#define SE_TRANSFORM_PARAMS

#include "PlatformConversions.h"


struct TransformData
{
	float4x4 g_model;
	float4x4 g_transposeInvModel;

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "TransformParams";
#endif
};


#endif // SE_TRANSFORM_PARAMS