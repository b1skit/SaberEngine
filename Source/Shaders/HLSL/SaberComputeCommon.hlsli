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


// TODO: If we're using UNORM or SNORM types with UAVs, we need to declare the resource as unorm/snorm
//	-> e.g. RWBuffer<unorm float> uav;
// https://learn.microsoft.com/en-us/windows/win32/direct3d12/typed-unordered-access-view-loads
// -> Build this in to the root sig parser

RWTexture2D<float4> output0 : register(u0);
RWTexture2D<float4> output1 : register(u1);
RWTexture2D<float4> output2 : register(u2);
RWTexture2D<float4> output3 : register(u3);
RWTexture2D<float4> output4 : register(u4);
RWTexture2D<float4> output5 : register(u5);
RWTexture2D<float4> output6 : register(u6);
RWTexture2D<float4> output7 : register(u7);


#endif // SABERCOMPUTECOMMON_HLSLI