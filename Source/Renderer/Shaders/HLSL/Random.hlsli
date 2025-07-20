// © 2025 Adam Badke. All rights reserved.
#ifndef RANDOM_HLSL
#define RANDOM_HLSL

#include "MathConstants.hlsli"

struct RNGState1D
{
	uint g_state;
};
struct RNGState2D
{
	uint2 g_state;
};
struct RNGState3D
{
	uint3 g_state;
};
struct RNGState4D
{
	uint4 g_state;
};


// PCG hash functions.
// As per "Hash Functions for GPU Rendering", Jarzynski, Olano, et al.
// Journal of Computer Graphics Techniques Vol. 9, No. 3, 2020
uint pcg1d(inout RNGState1D rngState)
{
	rngState.g_state = rngState.g_state * 747796405u + 2891336453u;
	uint word = ((rngState.g_state >> ((rngState.g_state >> 28u) + 4u)) ^ rngState.g_state) * 277803737u;
	return (word >> 22u) ^ word;
}

uint2 pcg2d(inout RNGState2D rngState)
{
	rngState.g_state = rngState.g_state * 1664525u + 1013904223u;
	uint2 word = rngState.g_state;
	word.x += word.y * 1664525u;
	word.y += word.x * 1664525u;
	word = word ^ (word >> 16u);
	word.x += word.y * 1664525u;
	word.y += word.x * 1664525u;
	word = word ^ (word >> 16u);
	return word;
}

uint3 pcg3d(RNGState3D rngState)
{
	rngState.g_state = rngState.g_state * 1664525u + 1013904223u;
	uint3 word = rngState.g_state;
	word.x += word.y * word.z;
	word.y += word.z * word.x;
	word.z += word.x * word.y;

	word ^= word >> 16u;

	word.x += word.y * word.z;
	word.y += word.z * word.x;
	word.z += word.x * word.y;

	return word;
}

uint4 pcg4d(RNGState4D rngState)
{
	rngState.g_state = rngState.g_state * 1664525u + 1013904223u;
	uint4 word = rngState.g_state;
	word.x += word.y * word.w;
	word.y += word.z * word.x;
	word.z += word.x * word.y;
	word.w += word.y * word.z;
	word = word ^ (word >> 16u);
	word.x += word.y * word.w;
	word.y += word.z * word.x;
	word.z += word.x * word.y;
	word.w += word.y * word.z;
	return word;
}


// ---

RNGState1D InitializeRNGState1D(uint3 seed) // Note: Prefer .z to be the most frequently changing value, e.g. frame number
{	
	RNGState1D rng;

	// Enable USE_LARGE_PRIMES to reduce correlation in high-resolution buffers
//#define USE_LARGE_PRIMES
#if defined(USE_LARGE_PRIMES)
	rng.g_state = seed.x * 73856093u ^ seed.y * 19349663u ^ seed.z * 83492791u;
#else
	rng.g_state = 19u * seed.x + 47u * seed.y + 101u * seed.z + 131u;
#endif    
	return rng;
}


RNGState1D InitializeRNGState1D(uint2 pixelCoord, uint frameNum)
{	
	return InitializeRNGState1D(uint3(pixelCoord, frameNum));
}


// Generate a random float in the range [0, 1)
float GetNextFloat(inout RNGState1D rng)
{	
	return float(pcg1d(rng)) / float(UINT_MAX);
}


// Generate a random UINT in the range [0, UINT_MAX]
uint GetNextUInt(inout RNGState1D rng)
{
	return pcg1d(rng);
}


// ---


RNGState2D InitializeRNGState2D(uint3 seed) // Note: Prefer .z to be the most frequently changing value, e.g. frame number
{
	RNGState2D rng;

	rng.g_state = uint2(
		seed.x * 73856093u ^ seed.z * 83492791u,
		seed.y * 19349663u ^ seed.z * 604931u);
	 
	return rng;
}


RNGState2D InitializeRNGState2D(uint2 pixelCoord, uint frameNum)
{
	return InitializeRNGState2D(uint3(pixelCoord, frameNum));
}


// Generate a random float in the range [0, 1)
float2 GetNextFloat2(inout RNGState2D rng)
{
	return float2(pcg2d(rng)) / float2(0xFFFFFFFF, 0xFFFFFFFF); // Return the normalized value
}


// Generate a random UINT in the range [0, UINT_MAX]
uint2 GetNextUInt2(inout RNGState2D rng)
{
	return pcg2d(rng);
}


#endif