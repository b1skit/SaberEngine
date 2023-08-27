// © 2023 Adam Badke. All rights reserved.
#ifndef NORMAL_MAP_UTILS_HLSL
#define NORMAL_MAP_UTILS_HLSL


float3 WorldNormalFromTextureNormal(float3 texNormal, float3x3 TBN)
{
	texNormal = normalize((texNormal * 2.f) - 1.f); // [0, 1] -> [-1, 1]
	return normalize(mul(TBN, texNormal));
}

#endif