// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_TRANSFORMATIONS
#define SABER_TRANSFORMATIONS


float3 GetWorldPos(float2 screenUV, float nonLinearDepth, float4x4 invViewProjection)
{
	float2 ndcXY = (screenUV * 2.f) - float2(1.f, 1.f); // [0,1] -> [-1, 1]

	// In SaberEngine, the (0, 0) UV origin is in the top-left, which means +Y is down in UV space.
	// In NDC, +Y is up and point (-1, -1) is in the bottom left.
	// Thus we must flip the Y coordinate here to compensate.
	ndcXY.y *= -1;

	const float4 ndcPos = float4(ndcXY.xy, nonLinearDepth, 1.f);

	float4 result = mul(invViewProjection, ndcPos);
	return result.xyz / result.w; // Apply the perspective division
}


#endif // SABER_TRANSFORMATIONS