// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_LIGHTING
#define SABER_LIGHTING

#include "MathConstants.hlsli"


// Specular D is the normal distribution function (NDF), which approximates the surface area of microfacets aligned with
// the halfway vector between the light and view directions.
// As per Disney this is the GGX/Trowbridge-Reitz NDF, with their roughness reparameterization of alpha = roughness^2
float SpecularD(float roughness, float NoH)
{
	const float alpha = roughness * roughness; // Disney reparameterization: alpha = roughness^2
	const float alpha2 = alpha * alpha;
	const float NoH2 = NoH * NoH;
	
	return alpha2 / max((M_PI * pow((NoH2 * (alpha2 - 1.f) + 1.f), 2.f)), FLT_MIN); // == 1/pi when roughness = 1
}


#endif