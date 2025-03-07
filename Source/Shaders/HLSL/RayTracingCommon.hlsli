// © 2025 Adam Badke. All rights reserved.


// Returns interpolation weights for v0, v1, v2 of a triangle
float3 GetBarycentricWeights(float2 bary)
{
	return float3(1.f - bary.x - bary.y, bary.x, bary.y);
}