// © 2023 Adam Badke. All rights reserved.
#ifndef SABER_LIGHTING
#define SABER_LIGHTING

#include "SaberGlobals.glsl"
#include "SaberCommon.glsl"



// Map linear roughness to "perceptually linear" roughness. 
// Perceptually linear roughness results in a linear-appearing transition from smooth to rough surfaces.
// As per p.13 of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al., we use the squared roughness
// remapping
float RemapRoughness(float linearRoughness)
{
	return linearRoughness * linearRoughness;
}

// Specular D: The normal distribution function (NDF)
// Trowbridge-Reitz GGX Normal Distribution Function: Approximate area of surface microfacets aligned with the halfway
// vector between the light and view dirs
float NDF(vec3 N, vec3 H, float roughness)
{
	// Disney reparameterizes roughness as alpha = roughness^2. Then the GGX/Trowbridge-Reitz NDF requires alpha^2
	const float alpha2 = pow(roughness, 4.f);

	const float nDotH = clamp(dot(N, H), 0.f, 1.f);
	const float nDotH2 = nDotH * nDotH;

	const float denominator = max((nDotH2 * (alpha2 - 1.f)) + 1.f, 0.0001);
	
	return alpha2 / (M_PI * denominator * denominator);
}


// Helper function for geometry function
float GeometrySchlickGGX(float NoV, float remappedRoughness)
{
	return NoV / ((NoV * (1.f - remappedRoughness)) + remappedRoughness);
}


// Specular G: 
// Geometry function: Compute the proportion of microfacets visible
float GeometryG(float NoV, float NoL, float remappedRoughness)
{
	float ggx1 = GeometrySchlickGGX(NoV, remappedRoughness);
	float ggx2 = GeometrySchlickGGX(NoL, remappedRoughness);
	
	return ggx1 * ggx2;
}


// Specular D is the normal distribution function (NDF), which approximates the surface area of microfacets aligned with
// the halfway vector between the light and view directions.
// As per Disney this is the GGX/Trowbridge-Reitz NDF, with their roughness reparameterization of alpha = roughness^2
float SpecularD(float remappedRoughness, float NoH)
{	
	// Note: Disney reparameterizes alpha = roughness^2. This is our remapping, so we pass it in here
	const float alpha = remappedRoughness; 
	const float alpha2 = alpha * alpha;
	const float NoH2 = NoH * NoH;
	
	return alpha2 / max((M_PI * pow((NoH2 * (alpha2 - 1.f) + 1.f), 2.f)), FLT_MIN); // == 1/pi when roughness = 1
}


// Compute the blended Fresnel reflectance at incident angles (i.e L == N).
// The linearAlbedo defines the diffuse albedo for non-metallic surfaces, and the Fresnel reflectance at normal
// incidence for metallic surfaces. Thus, the linearMetalness value is used to blend between these.
// Based on section B.3.5 "Metal BRDF and Dielectric BRDF" of the glTF 2.0 specifications
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metal-brdf-and-dielectric-brdf
vec3 ComputeBlendedF0(vec3 f0, vec3 linearAlbedo, float linearMetalness)
{
	return mix(f0, linearAlbedo, linearMetalness);
}


// Compute the F_D90 term (i.e. reflectivity at grazing angles) for the Schlick Fresnel approximation used in our
// Cook-Torrance microfacet specular BRDF. 
//	theta_d = LoH, the cosine of the angle between the light vector, and the micronormal (aka. the half vector).
// Based on equation 5 (p.9) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al; and 
// section 5.3 (p.14) of "Physically Based Shading at Disney", Burley.
float ComputeF90(float remappedRoughness, float LoH)
{
	return 0.5f + 2.f * remappedRoughness * LoH * LoH;
}


// Fresnel function F (Shlick approximation).
// Describes the amount of light reflected from a (smooth) surface at the interface between 2 media.
// f0 = Reflectance at normal incidence. 
//	f0 = (n_1 - n_2)^2 / (n_1 + n_2)^2, with n_i = the material's index of refraction (IOR). 
//	When one media is air, which has an IOR ~= 1, f0 = (n-1)^2 / (n+1)^2
// f90 = Maximum reflectance (i.e. at grazing incidence, when the normal and ray are 90 degrees apart)
// u = cosine of the angle between the surface normal N and the incident ray
vec3 FresnelSchlickF(in vec3 f0, in float f90, in float u)
{
	// Schuler's solution for specular micro-occlusion.
	// derived from f0 (which is itself derived from the diffuse color), based on the knowledge that no real material
	// has a reflectance < 2%. Values of reflectance < 0.02 are assumed to be the result of pre-baked occlusion, and 
	// used to smoothly decrease the Fresnel reflectance contribution
	// f90 = saturate(50.0 * dot( f0 , 0.33f) );
	
	return f0 + (vec3(f90, f90, f90) - f0) * pow(1.f - u, 5.f);
}


// Schlick's Approximation, with an added roughness term (as per Sebastien Lagarde)
vec3 FresnelSchlick_Roughness(float NoV, vec3 F0, float roughness)
{
	NoV = clamp(NoV, 0.f, 1.f);
	return F0 + (max(vec3(1.f - roughness), F0) - F0) * pow(1.f - NoV, 5.0);
}


// Compute the halfway vector between light and view dirs
vec3 HalfVector(vec3 light, vec3 view)
{
	return normalize(light + view);
}


vec3 ApplyExposure(vec3 linearColor, float exposure)
{
	return linearColor * exposure;
}


// Note: The original Disney diffuse model is not energy conserving. This implementation from Frostbite is a 
// modification that renormalizes it to make it _almost_ energy conserving
// Based on listing 1 (p.10) "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al.
float FrostbiteDisneyDiffuse(float NoV, float NoL, float LoH, float linearRoughness)
{
	const float energyBias		= mix(0.f, 0.5f, linearRoughness);
	const float energyFactor	= mix(1.f, 1.f / 1.51f, linearRoughness);
	const float fd90			= energyBias + 2.f * LoH * LoH * linearRoughness;
	const vec3 f0				= vec3(1.f, 1.f, 1.f);
	const float lightScatter	= FresnelSchlickF(f0, fd90, NoL).r;
	const float viewScatter		= FresnelSchlickF(f0, fd90, NoV).r;
	
	return lightScatter * viewScatter * energyFactor;
}


// Compute the diffuse color. For smooth, shiny metals we blend towards black as the specular contribution increases.
// Based on section B.3.5 "Metal BRDF and Dielectric BRDF" of the glTF 2.0 specifications
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metal-brdf-and-dielectric-brdf
vec3 ComputeDiffuseColor(vec3 linearAlbedo, vec3 f0, float metalness)
{
	return linearAlbedo * (vec3(1.f, 1.f, 1.f) - f0) * (1.f - metalness); // As per the GLTF specs
}


struct LightingParams
{
	vec3 LinearAlbedo;
	vec3 WorldNormal;
	float LinearRoughness;
	float RemappedRoughness;
	float LinearMetalness;
	vec3 WorldPosition;
	vec3 F0;
	vec3 LightWorldPos; // 0 for directional lights
	vec3 LightWorldDir;
	vec3 LightColor;
	float LightIntensity;
	float LightAttenuationFactor;
	float ShadowFactor;
	
	vec3 CameraWorldPos;
	float Exposure;

	float DiffuseScale; 
	float SpecularScale;
};


vec3 ComputeLighting(const LightingParams lightingParams)
{
	const vec3 N = normalize(lightingParams.WorldNormal);

	// World-space point -> camera direction
	const vec3 V = normalize(lightingParams.CameraWorldPos - lightingParams.WorldPosition); 
	const float NoV	= clamp(dot(N, V), FLT_EPSILON, 1.f); // Prevent NaNs at glancing angles

	const vec3 L = normalize(lightingParams.LightWorldDir);
	const float NoL = clamp(dot(N, L), FLT_EPSILON, 1.f); // Prevent NaNs at glancing angles

	const vec3 H = normalize(HalfVector(L, V));
	const float LoH = clamp(dot(L, H), 0.f, 1.f);

	const float diffuseResponse = FrostbiteDisneyDiffuse(NoV, NoL, LoH, lightingParams.LinearRoughness);

	const vec3 sunHue = lightingParams.LightColor;
	const float sunIlluminanceLux = lightingParams.LightIntensity;
	
	const vec3 illuminance = 
		sunIlluminanceLux * sunHue * NoL * lightingParams.LightAttenuationFactor * lightingParams.ShadowFactor;

	const vec3 dielectricSpecular = lightingParams.F0;
	const vec3 blendedF0 =
		ComputeBlendedF0(dielectricSpecular, lightingParams.LinearAlbedo, lightingParams.LinearMetalness);
	const vec3 diffuseReflectance = ComputeDiffuseColor(
		lightingParams.LinearAlbedo, 
		blendedF0, 
		lightingParams.LinearMetalness) * diffuseResponse * lightingParams.DiffuseScale;

	const float f90 = ComputeF90(lightingParams.LinearRoughness, LoH);
	const vec3 fresnelF = FresnelSchlickF(blendedF0, f90, LoH);

	const float geometryG = GeometryG(NoV, NoL, lightingParams.RemappedRoughness);

	const float NoH = clamp(dot(N, H), 0.f, 1.f);
	const float specularD = SpecularD(lightingParams.RemappedRoughness, NoH);

	const vec3 specularReflectance = fresnelF * geometryG * specularD * lightingParams.SpecularScale;
	
	const vec3 combinedContribution = (diffuseReflectance + specularReflectance) * illuminance;
	// Note: We're omitting the pi term in the albedo
	
	// Apply exposure:
	const vec3 exposedColor = ApplyExposure(combinedContribution, lightingParams.Exposure);

	return exposedColor;
}


// Shadow mapping
//---------------

// Compute a depth map bias value based on surface orientation
float GetSlopeScaleBias(float NoL)
{
	return max(g_shadowCamNearFarBiasMinMax.z, g_shadowCamNearFarBiasMinMax.w * (1.f - NoL));
}


// Find out if a fragment (in world space) is in shadow
float GetShadowFactor(vec3 shadowPos, sampler2D shadowMap, float NoL)
{
	// Convert Projection [-1, 1], -> Screen/UV [0,1] space.
	// Note: SaberEngine overrides the default OpenGL coordinate system (via glClipControl/GLM_FORCE_DEPTH_ZERO_TO_ONE),
	// so z is already in [0,1]
	vec3 shadowScreen = vec3((shadowPos.xy + 1.f) / 2.f, shadowPos.z); 
	shadowScreen.y = 1.f - shadowScreen.y; // UV (0,0) is in the top-left

	// Compute a slope-scaled bias depth:
	const float biasedDepth = shadowScreen.z - GetSlopeScaleBias(NoL);

	// Compute a block of samples around our fragment, starting at the top-left:
	const int gridSize = 4; // MUST be a power of two TODO: Compute this on C++ side and allow for uploading of arbitrary samples (eg. odd, even)

	const float offsetMultiplier = (float(gridSize) / 2.f) - 0.5;

	shadowScreen.x -= offsetMultiplier * g_shadowMapTexelSize.z;
	shadowScreen.y += offsetMultiplier * g_shadowMapTexelSize.w;

	float depthSum = 0;
	for (int row = 0; row < gridSize; row++)
	{
		for (int col = 0; col < gridSize; col++)
		{
			depthSum += (biasedDepth < texture(shadowMap, shadowScreen.xy).r ? 1.f : 0.f);

			shadowScreen.x += g_shadowMapTexelSize.z;
		}

		shadowScreen.x -= gridSize * g_shadowMapTexelSize.z;
		shadowScreen.y -= g_shadowMapTexelSize.w;
	}

	depthSum /= (gridSize * gridSize);

	return depthSum;
}


// Compute a soft shadow factor from a cube map.
// Based on Lengyel's Foundations of Game Engine Development Volume 2: Rendering, p164, listing 8.8
float GetShadowFactor(vec3 lightToFrag, samplerCube shadowMap, const float NoL)
{
	vec3 sampleDir = WorldToCubeSampleDir(lightToFrag);

	const float cubemapFaceResolution = g_shadowMapTexelSize.x; // Assume our shadow cubemap has square faces...	

	// Calculate non-linear, projected depth buffer depth from the light-to-fragment direction. The eye depth w.r.t
	// our cubemap view is the value of the largest component of this direction. Also apply a slope-scale bias.
	const vec3 absSampleDir = abs(sampleDir);
	const float maxXY = max(absSampleDir.x, absSampleDir.y);
	const float eyeDepth = max(maxXY, absSampleDir.z);
	const float biasedEyeDepth = eyeDepth - GetSlopeScaleBias(NoL);

	const float nonLinearDepth = 
		ConvertLinearDepthToNonLinear(g_shadowCamNearFarBiasMinMax.x, g_shadowCamNearFarBiasMinMax.y, biasedEyeDepth);

	// Compute a sample offset for PCF shadow samples:
	const float sampleOffset = 2.f / cubemapFaceResolution;

	// Calculate offset vectors:
	float offset = sampleOffset * eyeDepth;
	float dxy = (maxXY > absSampleDir.z) ? offset : 0.f;
	float dx = (absSampleDir.x > absSampleDir.y) ? dxy : 0.f;
	vec2 oxy = vec2(offset - dx, dx);
	vec2 oyz = vec2(offset - dxy, dxy);

	vec3 limit = vec3(eyeDepth, eyeDepth, eyeDepth);
	const float bias = 1.f / 1024.0; // Epsilon = 1/1024.

	limit.xy -= oxy * bias;
	limit.yz -= oyz * bias;

	// Get the center sample:
	float light = texture(shadowMap, sampleDir).r > nonLinearDepth ? 1.f : 0.f;

	// Get 4 extra samples at diagonal offsets:
	sampleDir.xy -= oxy;
	sampleDir.yz -= oyz;

	light += texture(shadowMap, clamp(sampleDir, -limit, limit)).r > nonLinearDepth ? 1.f : 0.f;
	sampleDir.xy += oxy * 2.f;

	light += texture(shadowMap, clamp(sampleDir, -limit, limit)).r > nonLinearDepth ? 1.f : 0.f;
	sampleDir.yz += oyz * 2.f;

	light += texture(shadowMap, clamp(sampleDir, -limit, limit)).r > nonLinearDepth ? 1.f : 0.f;
	sampleDir.xy -= oxy * 2.f;

	light += texture(shadowMap, clamp(sampleDir, -limit, limit)).r > nonLinearDepth ? 1.f : 0.f;

	return (light * 0.2);	// Return the average of our 5 samples
}


// Sampling:
//----------

// Helper function: Compute the Van der Corput sequence via radical inverse
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
vec2 Hammersley2D(uint i, uint N)
{
	return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}


// Create a uniformly-distributed hemisphere direction (Z-up) from the Hammersley 2D point
// Based on:  http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
vec3 HemisphereSample_uniformDist(float u, float v)
{
	const float phi = v * M_2PI;
	const float cosTheta = 1.f - u;
	const float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}
  

// Create a cosine-distributed hemisphere direction (Z-up) from the Hammersley 2D point (ie. For diffuse IBL sampling)
// Based on:  http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
vec3 HemisphereSample_cosineDist(vec2 u)
{
	const float phi = u.y * M_2PI;
	const float cosTheta = sqrt(1.f - u.x);
	const float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}


//  Get a sample vector near a microsurface's halfway vector, from input roughness and a low-discrepancy sequence value
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
	const float a = roughness * roughness;

	const float phi = M_2PI * Xi.x;
	const float cosTheta = sqrt((1.f - Xi.y) / (1.f + (a * a - 1.f) * Xi.y));
	const float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	// Convert spherical -> cartesian coordinates
	vec3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// from tangent-space vector to world-space sample vector
	const vec3 up = abs(N.z) < 0.999f ? vec3(0.f, 0.f, 1.f) : vec3(1.f, 0.f, 0.f);
	const vec3 tangent = normalize(cross(up, N));
	const vec3 bitangent = cross(N, tangent);

	const vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;

	return normalize(sampleVec);
}


#endif // SABER_LIGHTING