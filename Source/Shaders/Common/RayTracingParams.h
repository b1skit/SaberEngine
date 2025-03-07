// © 2025 Adam Badke. All rights reserved.
#ifndef SE_RAYTRACING_COMMON
#define SE_RAYTRACING_COMMON

#include "PlatformConversions.h"


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
};


#endif // SE_RAYTRACING_COMMON