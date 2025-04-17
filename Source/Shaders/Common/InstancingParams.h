// © 2024 Adam Badke. All rights reserved.
#ifndef SE_INSTANCING_PARAMS
#define SE_INSTANCING_PARAMS

#include "PlatformConversions.h"

// Arbitrary: The actual max is 4096 entries, where each entry can be a 4x 32-bit value
// https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-constants
// 16 entries fills out the minimum 256B required by the ConstantBuffer alignment rules
#define INSTANCE_ARRAY_SIZE 128

struct InstanceIndices
{
	uint g_transformIdx;
	uint g_materialIdx;

	uint2 _padding;
};

struct InstanceIndexData
{
	// Indexed by instance ID: .x = transform, .y = material, .zw = unused
	InstanceIndices g_instanceIndices[INSTANCE_ARRAY_SIZE];

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "InstanceIndexParams";
	static constexpr uint8_t k_maxInstances = INSTANCE_ARRAY_SIZE;
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


// Cleanup:
#undef INSTANCE_ARRAY_SIZE


#endif // SE_INSTANCING_PARAMS