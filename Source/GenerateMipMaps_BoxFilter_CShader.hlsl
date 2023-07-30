// © 2023 Adam Badke. All rights reserved.

// TODO: If we're sing UNORM or SNORM types with UAVs, we need to declare the resource as unorm/snorm
//	-> e.g. RWBuffer<unorm float> uav;
// https://learn.microsoft.com/en-us/windows/win32/direct3d12/typed-unordered-access-view-loads
// -> Build this in to the root sig parser

RWTexture2D<float4> output0 : register(u0);
RWTexture2D<float4> output1 : register(u1);
RWTexture2D<float4> output2 : register(u2);
RWTexture2D<float4> output3 : register(u3);


[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	output0[DTid.xy] = float4(1.f, 0.f, 0.f, 1.f);
	output1[DTid.xy] = float4(0.f, 1.f, 0.f, 1.f);
	output2[DTid.xy] = float4(0.f, 0.f, 1.f, 1.f);
	output3[DTid.xy] = float4(1.f, 1.f, 1.f, 1.f);
}