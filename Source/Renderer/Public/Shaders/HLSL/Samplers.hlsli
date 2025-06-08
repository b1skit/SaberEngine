// © 2024 Adam Badke. All rights reserved.
#ifndef SAMPLERS_HLSL
#define SAMPLERS_HLSL

SamplerState WrapMinMagLinearMipPoint;
SamplerState ClampMinMagLinearMipPoint;
SamplerState ClampMinMagMipPoint;
SamplerState WhiteBorderMinMagMipPoint;
SamplerState ClampMinMagMipLinear;
SamplerState WrapMinMagMipLinear;
SamplerState WrapAnisotropic;

// PCF Samplers
SamplerComparisonState BorderCmpMinMagLinearMipPoint;
SamplerComparisonState WrapCmpMinMagLinearMipPoint;

#endif