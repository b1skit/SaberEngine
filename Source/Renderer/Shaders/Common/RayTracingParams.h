// © 2025 Adam Badke. All rights reserved.
#ifndef SE_RAYTRACING_COMMON
#define SE_RAYTRACING_COMMON

#include "PlatformConversions.h"

#include "../Common/MaterialParams.h"


#if defined(__cplusplus)

// Mirrors the HLSL intrinsic RAY_FLAG enum passed by ray generation shader TraceRay() calls
// https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#types-enums-subobjects-and-concepts
enum RayFlag : uint32_t
{
	None						= 0,
	ForceOpaque					= 0x01,
	ForceNonOpaque				= 0x02,
	AcceptFirstHitAndEndSearch	= 0x04,
	SkipClosestHitShader		= 0x08,
	CullBackFacingTriangles		= 0x10,
	CullFrontFacingTriangles	= 0x20,
	CullOpaque					= 0x40,
	CullNonOpaque				= 0x80,
	SkipTriangles				= 0x100,
	SkipProceduralPrimitives	= 0x200,
};

inline RayFlag operator|(RayFlag lhs, RayFlag rhs)
{
	return static_cast<RayFlag>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}
inline RayFlag& operator|=(RayFlag& lhs, RayFlag rhs)
{
	return lhs = lhs | rhs;
};
inline RayFlag operator&(RayFlag lhs, RayFlag rhs)
{
	return static_cast<RayFlag>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}
inline RayFlag& operator&=(RayFlag& lhs, RayFlag rhs)
{
	return lhs = lhs & rhs;
};


#endif // __cplusplus


// ---


struct RootConstantData
{
	uint4 g_data;
};


struct VertexStreamLUTData
{
	uint4 g_posNmlTanUV0Index;	// .xyzw = Position, Normal, Tangent, TexCoord0 resource indexes
	uint4 g_UV1ColorIndex;		// .xyzw = TexCoord1, Color, 16-bit index, 32-bit index resource indexes

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "VertexStreamLUTs";
#endif
};


struct InstancedBufferLUTData
{
	// .x = Material resource idx, .y = Material buffer index, .z = Material type, .w = unused
	uint4 g_materialIndexes;

	// .x = Transform resource idx, .y = Transform buffer idx, .zw = unused
	uint4 g_transformIndexes;


#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "InstancedBufferLUTs";


	inline static void SetMaterialIndex_Unlit(uint32_t lutIdx, void* dst)
	{
		static_cast<InstancedBufferLUTData*>(dst)->g_materialIndexes.y = lutIdx;
		static_cast<InstancedBufferLUTData*>(dst)->g_materialIndexes.z = MAT_ID_GLTF_Unlit;		
	}

	inline static void SetMaterialIndex_PBRMetallicRoughness(uint32_t lutIdx, void* dst)
	{
		static_cast<InstancedBufferLUTData*>(dst)->g_materialIndexes.y = lutIdx;
		static_cast<InstancedBufferLUTData*>(dst)->g_materialIndexes.z = MAT_ID_GLTF_PBRMetallicRoughness;		
	}

	inline static void SetTransformIndex(uint32_t lutIdx, void* dst)
	{
		static_cast<InstancedBufferLUTData*>(dst)->g_transformIndexes.y = lutIdx;
	}
#endif
};


struct DescriptorIndexData
{
	// .x = VertexStreamLUTs, .y = InstancedBufferLUTs, .z = CameraParams, .w = target Texture2DRWFloat4 idx
	uint4 g_descriptorIndexes; 

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "DescriptorIndexes";
#endif
};


struct raypayload HitInfo_Experimental
{
	float4 g_colorAndDistance read(caller) write(caller, anyhit, closesthit, miss);
};


struct raypayload RTAO_HitInfo
{
	float g_visibility read(caller) write(caller, miss, anyhit);
};


struct raypayload PathTracer_HitInfo
{
	float4 g_colorAndDistance read(caller) write(caller, anyhit, closesthit, miss);
};


struct TraceRayData
{
	// .x = InstanceInclusionMask. Default = 0xFF (No geometry will be masked)
	// .y = RayContributionToHitGroupIndex (AKA ray type): Offset to apply when selecting hit groups for a ray. Default = 0.
	// .z = MultiplierForGeometryContributionToHitGroupIndex: > 1 Allows shaders for multiple ray types to be adjacent in SBT. Default = 0.
	// .w = MissShaderIndex: Index of miss shader to use when multiple consecutive miss shaders are present in the SBT
	uint4 g_traceRayParams;

	// .x = RayFlag, .yzw = unused
	uint4 g_rayFlags;


#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "TraceRayParams";
#endif
};


struct TraceRayInlineData
{
	// .x = InstanceInclusionMask. Default = 0xFF (No geometry will be masked). 
	// .y = RayFlags. The intention is to logically OR these with the shader's compile-time RayQuery's RAY_FLAGs
	// .zw = unused
	uint4 g_traceRayInlineParams;
	float4 g_rayParams; // .x = tMin, .y = length offset, .zw = unused


#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "TraceRayInlineParams";
#endif
};


#endif // SE_RAYTRACING_COMMON