// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_SAMPLING
#define SABER_SAMPLING

#include "MathConstants.hlsli"


// Compute the Van der Corput sequence (a simple 1D low-discrepancy sequence over the unit interval. 
// See https://en.wikipedia.org/wiki/Van_der_Corput_sequence) via radical inverse.
// Based on:  http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html (As per Hacker's Delight)
float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}


// Compute the i'th Hammersley point, of N points
// Based on:  http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 Hammersley2D(uint i, uint N)
{
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}


// Based on listing A.2 (p.106) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
void ImportanceSampleCosDir(
	in float2 u,
	in float3 N,
	in float3 up,
	out float3 L,
	out float NoL,
	out float pdf)
{
	// Build a local referencial:
	//const float3 up = abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
	const float3 tangentX = normalize(cross(up, N));
	const float3 tangentY = cross(N, tangentX);
	
	const float u1 = u.x;
	const float u2 = u.y;

	const float r = sqrt(u1);

	const float phi = u2 * M_2PI;

	L = float3(
		r * cos(phi),
		r * sin(phi),
		sqrt(max(0.0f, 1.0f - u1)));
	
	L = normalize((tangentX * L.y) + (tangentY * L.x) + (N * L.z));

	NoL = dot(L, N);

	pdf = NoL * M_1_PI;
}


void ImportanceSampleCosDir(
	in float2 u,
	in float3 N,
	out float3 L,
	out float NoL,
	out float pdf)
{
	// Try and assemble an arbitrary up vector. Note: This tends to give artifacts when N is near +/-Z, it's better to
	// supply a known valid up vector if possible
	const float3 up = abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
	
	ImportanceSampleCosDir(u, N, up, L, NoL, pdf);
}

#endif