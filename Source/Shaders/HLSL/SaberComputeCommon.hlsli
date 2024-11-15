// © 2023 Adam Badke. All rights reserved.
#ifndef SABERCOMPUTECOMMON_HLSLI
#define SABERCOMPUTECOMMON_HLSLI

struct ComputeIn
{
	uint3 GId	: SV_GroupID;			// Thread group index within the dispatch
	uint3 GTId	: SV_GroupThreadID;		// Local thread ID within the thread group
	uint3 DTId	: SV_DispatchThreadID;	// Global thread ID within the dispatch
	uint GIdx	: SV_GroupIndex;		// Flattened local index within the thread group
};


#endif // SABERCOMPUTECOMMON_HLSLI