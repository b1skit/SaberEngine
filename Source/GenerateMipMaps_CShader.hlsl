// © 2023 Adam Badke. All rights reserved.

RWTexture2D<float4> output;


[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	// TEMP HAX: Just output a color...
	output[DTid.xy] = float4(1.f, 0.f, 0.f, 1.f);
}