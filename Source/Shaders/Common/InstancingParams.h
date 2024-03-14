// © 2024 Adam Badke. All rights reserved.
#ifndef SE_INSTANCING_PARAMS
#define SE_INSTANCING_PARAMS

#include "PlatformConversions.h"


struct InstanceIndexParamsData
{
	uint g_transformIdx;
	uint g_materialIdx;

	uint2 _padding;

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "InstanceIndexParams";
#endif
};


struct InstancedTransformParamsData
{
	float4x4 g_model;
	float4x4 g_transposeInvModel;

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "InstancedTransformParams";
#endif
};


#endif // SE_INSTANCING_PARAMS