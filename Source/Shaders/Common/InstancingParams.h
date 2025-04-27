// © 2024 Adam Badke. All rights reserved.
#ifndef SE_INSTANCING_PARAMS
#define SE_INSTANCING_PARAMS

#include "PlatformConversions.h"


struct InstanceIndexData
{
	uint g_transformIdx;
	uint g_materialIdx;
	// Note: This is exclusively a StructuredBuffer, so we don't use any padding


#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "InstanceIndexParams";

	inline static void WriteTransformIndex(uint32_t lutIdx, void* dst)
	{
		static_cast<InstanceIndexData*>(dst)->g_transformIdx = lutIdx;
	}

	inline static void WriteMaterialIndex(uint32_t lutIdx, void* dst)
	{
		static_cast<InstanceIndexData*>(dst)->g_materialIdx = lutIdx;
	}
#endif
};


struct TransformData
{
	float4x4 g_model;
	float4x4 g_transposeInvModel;

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "InstancedTransformParams";
#endif
};


#endif // SE_INSTANCING_PARAMS