// © 2024 Adam Badke. All rights reserved.
#ifndef SE_LIGHT_PARAMS
#define SE_LIGHT_PARAMS

#include "PlatformConversions.h"


struct LightData
{
	float4 g_lightColorIntensity; // .rgb = hue, .a = intensity

	// .xyz = Point/spot lights: world pos. Directional lights: Normalized point -> source dir
	// .w = emitter radius (point/spot lights)
	float4 g_lightWorldPosRadius;
	float4 g_globalForwardDir; // .xyz = Local -Z (i.e. Direction light leaves the light source). .w = unused

	float4 g_intensityScale; // .xy = diffuse/specular intensity scale, .zw = spot light inner/outer angle

	float4x4 g_shadowCam_VP;

	float4 g_shadowMapTexelSize;	// .xyzw = width, height, 1/width, 1/height
	float4 g_shadowCamNearFarBiasMinMax; // .xy = shadow cam near/far, .zw = min, max shadow bias
	float4 g_shadowParams; // .x = has shadow (1.f), .y = quality mode, .zw = light size UV radius
	float4 g_renderTargetResolution; // .xy = xRes, yRes, .zw = 1/xRes 1/yRes

	// Type-specific extra values:
	// Point, directional: Unused
	// Spot: .xyz = pre-computed attenuation values: .x = cos(outerAngle), .y = scaleTerm, .z = offsetTerm
	float4 g_extraParams; 

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "LightParams";
#endif
};


struct AmbientLightData
{
	// .x = max PMREM mip level, .y = pre-integrated DFG texture width/height, .z diffuse scale, .w = specular scale
	float4 g_maxPMREMMipDFGResScaleDiffuseScaleSpec;
	float4 g_ssaoTexDims; // .xyzw = width, height, 1/width, 1/height

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "AmbientLightParams";
#endif
};


struct PoissonSampleParamsData
{
	float4 g_poissonSamples64[32]; // 64x float2
	float4 g_poissonSamples32[16]; // 32x float2
	float4 g_poissonSamples25[13]; // 25x float2

#if defined(__cplusplus)
	static constexpr char const* const s_shaderName = "PoissonSampleParams";

	// TODO: Dynamically generate these values. For now, we just hard code them

	static constexpr std::array<glm::vec2, 64> k_poissonSamples64 = {
		glm::vec2{ -0.934812, 0.366741 },
		glm::vec2{ -0.918943, -0.0941496 },
		glm::vec2{ -0.873226, 0.62389 },
		glm::vec2{ -0.8352, 0.937803 },
		glm::vec2{ -0.822138, -0.281655 },
		glm::vec2{ -0.812983, 0.10416 },
		glm::vec2{ -0.786126, -0.767632 },
		glm::vec2{ -0.739494, -0.535813 },
		glm::vec2{ -0.681692, 0.284707 },
		glm::vec2{ -0.61742, -0.234535 },
		glm::vec2{ -0.601184, 0.562426 },
		glm::vec2{ -0.607105, 0.847591 },
		glm::vec2{ -0.581835, -0.00485244 },
		glm::vec2{ -0.554247, -0.771111 },
		glm::vec2{ -0.483383, -0.976928 },
		glm::vec2{ -0.476669, -0.395672 },
		glm::vec2{ -0.439802, 0.362407 },
		glm::vec2{ -0.409772, -0.175695 },
		glm::vec2{ -0.367534, 0.102451 },
		glm::vec2{ -0.35313, 0.58153 },
		glm::vec2{ -0.341594, -0.737541 },
		glm::vec2{ -0.275979, 0.981567 },
		glm::vec2{ -0.230811, 0.305094 },
		glm::vec2{ -0.221656, 0.751152 },
		glm::vec2{ -0.214393, -0.0592364 },
		glm::vec2{ -0.204932, -0.483566 },
		glm::vec2{ -0.183569, -0.266274 },
		glm::vec2{ -0.123936, -0.754448 },
		glm::vec2{ -0.0859096, 0.118625 },
		glm::vec2{ -0.0610675, 0.460555 },
		glm::vec2{ -0.0234687, -0.962523 },
		glm::vec2{ -0.00485244, -0.373394 },
		glm::vec2{ 0.0213324, 0.760247 },
		glm::vec2{ 0.0359813, -0.0834071 },
		glm::vec2{ 0.0877407, -0.730766 },
		glm::vec2{ 0.14597, 0.281045 },
		glm::vec2{ 0.18186, -0.529649 },
		glm::vec2{ 0.188208, -0.289529 },
		glm::vec2{ 0.212928, 0.063509 },
		glm::vec2{ 0.23661, 0.566027 },
		glm::vec2{ 0.266579, 0.867061 },
		glm::vec2{ 0.320597, -0.883358 },
		glm::vec2{ 0.353557, 0.322733 },
		glm::vec2{ 0.404157, -0.651479 },
		glm::vec2{ 0.410443, -0.413068 },
		glm::vec2{ 0.413556, 0.123325 },
		glm::vec2{ 0.46556, -0.176183 },
		glm::vec2{ 0.49266, 0.55388 },
		glm::vec2{ 0.506333, 0.876888 },
		glm::vec2{ 0.535875, -0.885556 },
		glm::vec2{ 0.615894, 0.0703452 },
		glm::vec2{ 0.637135, -0.637623 },
		glm::vec2{ 0.677236, -0.174291 },
		glm::vec2{ 0.67626, 0.7116 },
		glm::vec2{ 0.686331, -0.389935 },
		glm::vec2{ 0.691031, 0.330729 },
		glm::vec2{ 0.715629, 0.999939 },
		glm::vec2{ 0.8493, -0.0485549 },
		glm::vec2{ 0.863582, -0.85229 },
		glm::vec2{ 0.890622, 0.850581 },
		glm::vec2{ 0.898068, 0.633778 },
		glm::vec2{ 0.92053, -0.355693 },
		glm::vec2{ 0.933348, -0.62981 },
		glm::vec2{ 0.95294, 0.156896 },
	};

	static constexpr std::array<glm::vec2, 32> k_poissonSamples32 =
	{
		glm::vec2{ -0.975402, -0.0711386 },
		glm::vec2{ -0.920347, -0.41142 },
		glm::vec2{ -0.883908, 0.217872 },
		glm::vec2{ -0.884518, 0.568041 },
		glm::vec2{ -0.811945, 0.90521 },
		glm::vec2{ -0.792474, -0.779962 },
		glm::vec2{ -0.614856, 0.386578 },
		glm::vec2{ -0.580859, -0.208777 },
		glm::vec2{ -0.53795, 0.716666 },
		glm::vec2{ -0.515427, 0.0899991 },
		glm::vec2{ -0.454634, -0.707938 },
		glm::vec2{ -0.420942, 0.991272 },
		glm::vec2{ -0.261147, 0.588488 },
		glm::vec2{ -0.211219, 0.114841 },
		glm::vec2{ -0.146336, -0.259194 },
		glm::vec2{ -0.139439, -0.888668 },
		glm::vec2{ 0.0116886, 0.326395 },
		glm::vec2{ 0.0380566, 0.625477 },
		glm::vec2{ 0.0625935, -0.50853 },
		glm::vec2{ 0.125584, 0.0469069 },
		glm::vec2{ 0.169469, -0.997253 },
		glm::vec2{ 0.320597, 0.291055 },
		glm::vec2{ 0.359172, -0.633717 },
		glm::vec2{ 0.435713, -0.250832 },
		glm::vec2{ 0.507797, -0.916562 },
		glm::vec2{ 0.545763, 0.730216 },
		glm::vec2{ 0.56859, 0.11655 },
		glm::vec2{ 0.743156, -0.505173 },
		glm::vec2{ 0.736442, -0.189734 },
		glm::vec2{ 0.843562, 0.357036 },
		glm::vec2{ 0.865413, 0.763726 },
		glm::vec2{ 0.872005, -0.927 },
	};

	static constexpr std::array<glm::vec2, 25> k_poissonSamples25 =
	{
		glm::vec2{ -0.978698, -0.0884121 },
		glm::vec2{ -0.841121, 0.521165 },
		glm::vec2{ -0.71746, -0.50322 },
		glm::vec2{ -0.702933, 0.903134 },
		glm::vec2{ -0.663198, 0.15482 },
		glm::vec2{ -0.495102, -0.232887 },
		glm::vec2{ -0.364238, -0.961791 },
		glm::vec2{ -0.345866, -0.564379 },
		glm::vec2{ -0.325663, 0.64037 },
		glm::vec2{ -0.182714, 0.321329 },
		glm::vec2{ -0.142613, -0.0227363 },
		glm::vec2{ -0.0564287, -0.36729 },
		glm::vec2{ -0.0185858, 0.918882 },
		glm::vec2{ 0.0381787, -0.728996 },
		glm::vec2{ 0.16599, 0.093112 },
		glm::vec2{ 0.253639, 0.719535 },
		glm::vec2{ 0.369549, -0.655019 },
		glm::vec2{ 0.423627, 0.429975 },
		glm::vec2{ 0.530747, -0.364971 },
		glm::vec2{ 0.566027, -0.940489 },
		glm::vec2{ 0.639332, 0.0284127 },
		glm::vec2{ 0.652089, 0.669668 },
		glm::vec2{ 0.773797, 0.345012 },
		glm::vec2{ 0.968871, 0.840449 },
		glm::vec2{ 0.991882, -0.657338 },
	};
#endif
};


#endif // SE_LIGHT_PARAMS