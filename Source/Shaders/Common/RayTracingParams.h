// © 2025 Adam Badke. All rights reserved.
#ifndef SE_RAYTRACING_COMMON
#define SE_RAYTRACING_COMMON

#include "PlatformConversions.h"

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

#endif


struct raypayload HitInfo_Experimental
{
	float4 colorAndDistance read(caller) write(caller, anyhit, closesthit, miss);
};


struct TraceRayData
{
	// .x = InstanceInclusionMask. Default = 0xFF (No geometry will be masked)
	// .y = RayContributionToHitGroupIndex (AKA ray type): Offset to apply when selecting hit groups for a ray. Default = 0.
	// .z = MultiplierForGeometryContributionToHitGroupIndex: > 1 Allows shaders for multiple ray types to be adjacent in SBT. Default = 0.
	// .w = MissShaderIndex: Index of miss shader to use when multiple consecutive miss shaders are present in the SBT
	uint4 g_traceRayParams;

	uint4 g_rayFlags; // .x = RAY_FLAG, .yzw = unused
};


#endif // SE_RAYTRACING_COMMON