// � 2023 Adam Badke. All rights reserved.
#ifndef SABER_LIGHTING
#define SABER_LIGHTING

#include "SaberGlobals.glsl"
#include "SaberCommon.glsl"

// Saber Engine Lighting Common
// Defines lighting functions common to all shaders



// PBR Lighting:
//--------------

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


// Remap roughness for the geometry function
// This adjustment should only be used for analytic light sources. If applied to IBL, the results will be too dark
// at glancing angles.
float RemapRoughnessDirect(float roughness)
{
	// Non-linear remap [0,1] -> [0.125, 0.5] (https://www.desmos.com/calculator/mtb0ffbl82)

	float numerator = (roughness + 1.f);
	numerator *= numerator;

	return numerator / 8.f;
}


// Remap roughness for the geometry function, when computing image-based lighting contributions
float RemapRoughnessIBL(float roughness)
{
	return roughness * roughness;
}


// Helper function for geometry function
float GeometrySchlickGGX(float NoV, float remappedRoughness)
{
	const float nom = NoV;
	const float denom = (NoV * (1.f - remappedRoughness)) + remappedRoughness;
	
	return nom / denom;
}


// Specular G: 
// Geometry function: Compute the proportion of microfacets visible
float GeometryG(float NoV, float NoL, float remappedRoughness)
{
	float ggx1 = GeometrySchlickGGX(NoV, remappedRoughness);
	float ggx2 = GeometrySchlickGGX(NoL, remappedRoughness);
	
	return ggx1 * ggx2;
}


// Schlick's Approximation: Contribution of Fresnel factor in specular reflection
vec3 FresnelSchlick(float NoV, vec3 F0)
{
	return F0 + ((1.f - F0) * pow(1.f - NoV, 5.0));
}


// Schlick's Approximation, with an added roughness term (as per Sebastien Lagarde)
vec3 FresnelSchlick_Roughness(float NoV, vec3 F0, float roughness)
{
	NoV = max(NoV, 0.f);
	return F0 + (max(vec3(1.f - roughness), F0) - F0) * pow(1.f - NoV, 5.0);
}


// Compute the halfway vector between light and view dirs
vec3 HalfVector(vec3 light, vec3 view)
{
	vec3 halfVector = light + view;
	halfVector = normalize(halfVector);

	return halfVector;
}


vec3 ApplyExposure(vec3 linearColor, float exposure)
{
	return linearColor * exposure;
}


// Calculate attenuation based on distance between fragment and light
float LightAttenuation(vec3 fragWorldPosition, vec3 lightWorldPosition)
{
	const float lightDist = length(lightWorldPosition - fragWorldPosition);

	const float attenuation = 1.f / (1.f + (lightDist * lightDist));

	return attenuation;
}


struct LightingParams
{
	vec3 LinearAlbedo;
	vec3 WorldNormal;
	float LinearRoughness;
	float LinearMetalness;
	float AO;
	vec3 WorldPosition;
	vec3 F0;
	vec3 LightWorldPos; // == WorldPosition for directional lights, to ensure attenuation = 0
	vec3 LightWorldDir;
	vec3 LightColor;
	float ShadowFactor;
	mat4 View;
};

// General PBR lighting: Called from specific deferred light shaders
vec3 ComputeLighting(const LightingParams lightingParams)
{
	const vec3 lightWorldDir = normalize(lightingParams.LightWorldDir);
	const vec3 worldNormal = normalize(lightingParams.WorldNormal);

	const vec4 viewPosition = lightingParams.View * vec4(lightingParams.WorldPosition, 1.f); // View-space position
	const vec3 viewEyeDir = normalize(-viewPosition.xyz);	// View-space: Point -> eye/camera direction

	const mat3 viewRotationScale = mat3(lightingParams.View);
	const vec3 viewNormal = normalize(viewRotationScale * worldNormal); // View-space normal

	const vec3 lightViewDir = viewRotationScale * lightWorldDir;
	const vec3 halfVectorView = HalfVector(lightViewDir, viewEyeDir); // View-space half direction

	const float NoV	= max(0.f, dot(viewNormal, viewEyeDir) );
	const float NoL = max(0.f, dot(worldNormal, lightWorldDir));

	// Fresnel-Schlick approximation is only defined for non-metals, so we blend it here. Lerp blends towards albedo for metals
	const vec3 blendedF0 = mix(lightingParams.F0, lightingParams.LinearAlbedo, lightingParams.LinearMetalness); 

	const vec3 fresnel = FresnelSchlick(NoV, blendedF0);
	
	const float NDF = NDF(viewNormal, halfVectorView, lightingParams.LinearRoughness);

	const float remappedRoughness = RemapRoughnessDirect(lightingParams.LinearRoughness);
	const float geometry = GeometryG(NoV, NoL, remappedRoughness);

	const float diffuseScale = g_intensityScale.x;
	const float specularScale = g_intensityScale.y;

	// Specular:
	const vec3 specularContribution = specularScale * (NDF * fresnel * geometry) / max((4.0 * NoV * NoL), 0.0001f);
	
	// Diffuse:
	vec3 k_d = vec3(1.f) - fresnel;
	k_d = k_d * (1.f - lightingParams.LinearMetalness); // Metallics absorb refracted light
//	const vec3 diffuseContribution = k_d * lightingParams.LinearAlbedo.rgb; // Note: Omitted the "/ M_PI" factor here
	const vec3 diffuseContribution = diffuseScale * k_d * lightingParams.LinearAlbedo.rgb / M_PI;

	// Light attenuation:
	const float lightAttenuation = LightAttenuation(lightingParams.WorldPosition, lightingParams.LightWorldPos);
	const vec3 attenuatedLightColor = lightingParams.LightColor * lightAttenuation;

	const vec3 combinedContribution = 
		(diffuseContribution + specularContribution) * attenuatedLightColor * NoL * lightingParams.ShadowFactor;

	// Apply exposure:
	const vec3 exposedColor = ApplyExposure(combinedContribution, g_exposureProperties.x);

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