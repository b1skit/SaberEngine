// © 2025 Adam Badke. All rights reserved.
#ifndef SE_RAYTRACING_COMMON
#define SE_RAYTRACING_COMMON

#include "PlatformConversions.h"


struct raypayload HitInfo_Experimental
{
	float4 colorAndDistance read(caller) write(caller, anyhit, closesthit, miss);
};

#endif // SE_RAYTRACING_COMMON