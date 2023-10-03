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


// Compute the dominant direction for sampling a Disney diffuse retro-reflection lobe from the IEM probe. The 
// Based on listing 23 (p.70) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
float3 GetDiffuseDominantDir(float3 N, float3 V, float NoV, float roughness)
{
	const float a = 1.02341f * roughness - 1.51174f;
	const float b = -0.511705f * roughness + 0.755868f;
	const float lerpFactor = saturate((NoV * a + b) * roughness);
	
	return lerp(N, V, lerpFactor); // Don't normalize as this vector is for sampling a cubemap
}


// A referential RHCS, with N equivalent to Z
struct Referential
{
	float3 N;			// Equivalent to Z in a RHCS
	float3 TangentX;	// Equivalent to right/X in a RHCS
	float3 BitangentY;	// Equivalent to up/Y in an RHCS
};

// Build a referential coordinate system with respect to a normal vector
Referential BuildReferential(float3 N, float3 up)
{
	Referential referential;
	referential.N = N;
	
	referential.TangentX = normalize(cross(up, N));
	referential.BitangentY = cross(N, referential.TangentX);
	
	return referential;
}


// Constructs a best-guess up vector
Referential BuildReferential(float3 N)
{
	const float3 up = abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
	return BuildReferential(N, up);
}


// Compute a cosine-weighted sample direction
// Based on listing A.2 (p.106) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
void ImportanceSampleCosDir(
	in float2 u,
	in Referential localReferential,
	out float3 L,
	out float NoL,
	out float pdf)
{
	const float u1 = u.x;
	const float u2 = u.y;

	const float r = sqrt(u1);

	const float phi = u2 * M_2PI;

	L = float3(
		r * cos(phi),
		r * sin(phi),
		sqrt(max(0.0f, 1.f - u1)));
	
	L = normalize((localReferential.TangentX * L.y) + (localReferential.BitangentY * L.x) + (localReferential.N * L.z));

	NoL = dot(L, localReferential.N);

	pdf = NoL * M_1_PI;
}


void ImportanceSampleCosDir(
	in float2 u,
	in float3 N,
	out float3 L,
	out float NoL,
	out float pdf)
{	
	Referential localReferential = BuildReferential(N);
	ImportanceSampleCosDir(u, localReferential, L, NoL, pdf);
}


void ImportanceSampleGGXDir(
	in float2 u, in float roughness, in Referential localReferential, out float3 H)
{
	const float a = roughness * roughness;
	
	const float phi = M_2PI * u.x;
	const float cosTheta = sqrt((1.f - u.y) / (1.f + (a * a - 1.f) * u.y));
	const float sinTheta = sqrt(1.f - cosTheta * cosTheta);
	
	// Spherical to cartesian coordinates:
	H = float3(
		sinTheta * cos(phi),
		sinTheta * sin(phi),
		cosTheta);
	
	// Tangent-space to world-space:
	H = normalize(localReferential.TangentX * H.x + localReferential.BitangentY * H.y + localReferential.N * H.z);
}


void ImportanceSampleGGXDir(in float2 u, in float roughness, in float3 N, out float3 H)
{
	Referential localReferential = BuildReferential(N);
	ImportanceSampleGGXDir(u, roughness, localReferential, H);
}

#endif