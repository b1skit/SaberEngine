// © 2024 Adam Badke. All rights reserved.
#ifndef SAMPLERS_HLSL
#define SAMPLERS_HLSL

SamplerState WrapMinMagLinearMipPoint : register(s0, space0);
SamplerState ClampMinMagLinearMipPoint : register(s1, space0);
SamplerState ClampMinMagMipPoint : register(s2, space0);
SamplerState WhiteBorderMinMagMipPoint : register(s3, space0);
SamplerState ClampMinMagMipLinear : register(s4, space0);
SamplerState WrapMinMagMipLinear : register(s5, space0);
SamplerState WrapAnisotropic : register(s6, space0);

// PCF Samplers
SamplerComparisonState BorderCmpMinMagLinearMipPoint : register(s7, space0);
SamplerComparisonState WrapCmpMinMagLinearMipPoint : register(s8, space0);

#endif