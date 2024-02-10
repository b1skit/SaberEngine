// © 2024 Adam Badke. All rights reserved.
#ifndef XEGTAO_COMMON
#define XEGTAO_COMMON

#define SPECIFY_OWN_COMPUTE_OUTPUTS
#include "SaberComputeCommon.hlsli"


// XeGTAO.hlsli defines:
#define VA_SATURATE saturate
#include "XeGTAO.hlsli"


// As per the GTAO docs, these can be changed. In SE, we need to make sure to keep these shader-side defines in sync
// with the values defined in XeGTAO.h, as we use them directly to compute dispatch dimensions in XeGTAOGraphicsSystem
#define XE_GTAO_NUMTHREADS_X 8
#define XE_GTAO_NUMTHREADS_Y 8

// Buffers/parameter blocks:
struct SEXeGTAOSettingsCB
{
	float g_enabled; // Boolean: Output 1.f (white) if disabled, AO otherwise
	float3 _padding;
};
ConstantBuffer<SEXeGTAOSettingsCB> SEXeGTAOSettings;

// Note: struct GTAOConstants is already defined for us in XeGTAO.hlsli
ConstantBuffer<GTAOConstants> SEGTAOConstants;


// Samplers:
SamplerState ClampMinMagMipPoint;


// Texture inputs:
Texture2D<uint> g_srcHilbertLUT;

// Multiple main passes:
#if defined(XEGTAO_MAIN_PASS)

RWTexture2D<uint> output0 : register(u0);
RWTexture2D<unorm float> output1 : register(u1);

Texture2D<float4> GBufferWorldNormal;
Texture2D<lpfloat> PrefilteredDepth;

#endif // XEGTAO_MAIN_PASS


// Multiple denoise passes:
#if defined(XEGTAO_DENOISE_PASS)

Texture2D<uint> SourceAO;
Texture2D<lpfloat> SourceEdges;

// We need to define our own output0 due to the uint type
RWTexture2D<uint> output0 : register(u0);

#endif // XEGTAO_DENOISE_PASS





// Get screen or temporal noise for the Hilbert LUT
lpfloat2 SpatioTemporalNoise(uint2 pixCoord, uint temporalIndex)    // without TAA, temporalIndex is always 0
{
	float2 noise;
	
	uint index = g_srcHilbertLUT.Load(uint3(pixCoord % 64, 0)).x;
	
	// why 288? tried out a few and that's the best so far (with XE_HILBERT_LEVEL 6U) - but there's probably better :)
	index += 288 * (temporalIndex % 64);
	
	// R2 sequence - see http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
	return lpfloat2(frac(0.5 + index * float2(0.75487766624669276005, 0.5698402909980532659114)));
}


#endif // XEGTAO_COMMON