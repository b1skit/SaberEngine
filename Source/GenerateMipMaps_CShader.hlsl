// © 2023 Adam Badke. All rights reserved.

// TODO: If we're sing UNORM or SNORM types with UAVs, we need to declare the resource as unorm/snorm
//	-> e.g. RWBuffer<unorm float> uav;
// https://learn.microsoft.com/en-us/windows/win32/direct3d12/typed-unordered-access-view-loads
// -> Build this in to the root sig parser
RWTexture2D<float4> output; // TODO: Support dynamic names (this is currently hard-coded!!!!)


[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	// TEMP HAX: Just output a color...
	output[DTid.xy] = float4(1.f, 0.f, 1.f, 1.f);
}