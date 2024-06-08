// © 2024 Adam Badke. All rights reserved.
#include "LightParamsHelpers.h"
#include "Texture.h"
#include "TextureTarget.h"

#include "Shaders/Common/LightParams.h"


namespace gr
{
	AmbientLightData GetAmbientLightParamsData(
		uint32_t numPMREMMips, float diffuseScale, float specularScale, const uint32_t dfgTexWidthHeight, re::Texture const* ssaoTex)
	{

		AmbientLightData ambientLightParamsData{};

		assert(numPMREMMips > 0); // Don't underflow!
		const uint32_t maxPMREMMipLevel = numPMREMMips - 1;

		ambientLightParamsData.g_maxPMREMMipDFGResScaleDiffuseScaleSpec = glm::vec4(
			maxPMREMMipLevel,
			dfgTexWidthHeight,
			diffuseScale,
			specularScale);

		ambientLightParamsData.g_ssaoTexDims = glm::vec4(0.f);
		if (ssaoTex)
		{
			ambientLightParamsData.g_ssaoTexDims = ssaoTex->GetTextureDimenions();
		}

		return ambientLightParamsData;
	}


	LightData GetLightParamData(
		void const* lightRenderData,
		gr::Light::Type lightType,
		gr::Transform::RenderData const& transformData,
		gr::ShadowMap::RenderData const* shadowData,
		gr::Camera::RenderData const* shadowCamData,
		re::TextureTargetSet const* targetSet)
	{
		SEAssert(lightType != gr::Light::Type::AmbientIBL,
			"Ambient lights do not use the LightData structure");

		SEAssert((shadowData != nullptr) == (shadowCamData != nullptr),
			"Shadow data and shadow camera data depend on each other");

		LightData lightParams;
		memset(&lightParams, 0, sizeof(LightData)); // Ensure unused elements are zeroed

		// Direction the light is emitting from the light source. SE uses a RHCS, so this is the local -Z direction
		lightParams.g_globalForwardDir = glm::vec4(transformData.m_globalForward * -1.f, 0.f);

		// Set type-specific params:
		bool hasShadow = false;
		bool shadowEnabled = false;
		bool diffuseEnabled = false;
		bool specEnabled = false;
		glm::vec4 intensityScale(0.f); // Packed below as we go
		glm::vec4 extraParams(0.f);
		switch (lightType)
		{
		case gr::Light::Type::Directional:
		{
			gr::Light::RenderDataDirectional const* directionalData =
				static_cast<gr::Light::RenderDataDirectional const*>(lightRenderData);

			SEAssert((directionalData->m_hasShadow == (shadowData != nullptr)) &&
				(directionalData->m_hasShadow == (shadowCamData != nullptr)),
				"A shadow requires both shadow and camera data");

			lightParams.g_lightColorIntensity = directionalData->m_colorIntensity;

			// As per the KHR_lights_punctual, directional lights are at infinity and emit light in the direction of the
			// local -Z axis. Thus, this direction is pointing towards the source of the light (saves a * -1 on the GPU)
			// https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#directional
			lightParams.g_lightWorldPosRadius = glm::vec4(transformData.m_globalForward, 0.f); // WorldPos == Dir to light

			hasShadow = directionalData->m_hasShadow;
			shadowEnabled = hasShadow && shadowData->m_shadowEnabled;
			diffuseEnabled = directionalData->m_diffuseEnabled;
			specEnabled = directionalData->m_specularEnabled;
		}
		break;
		case gr::Light::Type::Point:
		{
			gr::Light::RenderDataPoint const* pointData =
				static_cast<gr::Light::RenderDataPoint const*>(lightRenderData);

			SEAssert((pointData->m_hasShadow == (shadowData != nullptr)) &&
				(pointData->m_hasShadow == (shadowCamData != nullptr)),
				"A shadow requires both shadow and camera data");

			lightParams.g_lightColorIntensity = pointData->m_colorIntensity;

			lightParams.g_lightWorldPosRadius =
				glm::vec4(transformData.m_globalPosition, pointData->m_emitterRadius);

			hasShadow = pointData->m_hasShadow;
			shadowEnabled = hasShadow && shadowData->m_shadowEnabled;
			diffuseEnabled = pointData->m_diffuseEnabled;
			specEnabled = pointData->m_specularEnabled;
		}
		break;
		case gr::Light::Type::Spot:
		{
			gr::Light::RenderDataSpot const* spotData = static_cast<gr::Light::RenderDataSpot const*>(lightRenderData);

			SEAssert((spotData->m_hasShadow == (shadowData != nullptr)) &&
				(spotData->m_hasShadow == (shadowCamData != nullptr)),
				"A shadow requires both shadow and camera data");

			lightParams.g_lightColorIntensity = spotData->m_colorIntensity;

			lightParams.g_lightWorldPosRadius = glm::vec4(transformData.m_globalPosition, spotData->m_emitterRadius);

			hasShadow = spotData->m_hasShadow;
			shadowEnabled = hasShadow && shadowData->m_shadowEnabled;
			diffuseEnabled = spotData->m_diffuseEnabled;
			specEnabled = spotData->m_specularEnabled;

			intensityScale.z = spotData->m_innerConeAngle;
			intensityScale.w = spotData->m_outerConeAngle;

			// Extra params:
			const float cosInnerAngle = glm::cos(spotData->m_innerConeAngle);
			const float cosOuterAngle = glm::cos(spotData->m_outerConeAngle);

			constexpr float k_divideByZeroEpsilon = 1.0e-5f;
			const float scaleTerm = 1.f / std::max(cosInnerAngle - cosOuterAngle, k_divideByZeroEpsilon);

			const float offsetTerm = -cosOuterAngle * scaleTerm;

			extraParams.x = cosOuterAngle;
			extraParams.y = scaleTerm;
			extraParams.z = offsetTerm;
		}
		break;
		default:
			SEAssertF("Light type does not use this buffer");
		}

		intensityScale.x = diffuseEnabled;
		intensityScale.y = specEnabled;

		lightParams.g_intensityScale = intensityScale;

		// Shadow params:
		if (hasShadow)
		{
			switch (lightType)
			{
			case gr::Light::Type::Directional:
			{
				lightParams.g_shadowCam_VP = shadowCamData->m_cameraParams.g_viewProjection;
			}
			break;
			case gr::Light::Type::Point:
			{
				lightParams.g_shadowCam_VP = glm::mat4(0.0f); // Unused by point light cube map shadows
			}
			break;
			case gr::Light::Type::Spot:
			{
				lightParams.g_shadowCam_VP = shadowCamData->m_cameraParams.g_viewProjection;
			}
			break;
			default:
				SEAssertF("Light shadow type does not use this buffer");
			}

			lightParams.g_shadowMapTexelSize = shadowData->m_textureDims;

			lightParams.g_shadowCamNearFarBiasMinMax = glm::vec4(
				shadowCamData->m_cameraConfig.m_near,
				shadowCamData->m_cameraConfig.m_far,
				shadowData->m_minMaxShadowBias);

			lightParams.g_shadowParams = glm::vec4(
				static_cast<float>(shadowEnabled),
				static_cast<float>(shadowData->m_shadowQuality),
				shadowData->m_softness, // [0,1] uv radius X
				shadowData->m_softness); // [0,1] uv radius Y
		}
		else
		{
			lightParams.g_shadowCam_VP = glm::mat4(0.0f);
			lightParams.g_shadowMapTexelSize = glm::vec4(0.f);
			lightParams.g_shadowCamNearFarBiasMinMax = glm::vec4(0.f);
			lightParams.g_shadowParams = glm::vec4(0.f);
		}

		lightParams.g_extraParams = extraParams;

		return lightParams;
	}


	PoissonSampleParamsData GetPoissonSampleParamsData()
	{
		PoissonSampleParamsData shadowSampleParams{};

		memcpy(shadowSampleParams.g_poissonSamples64,
			PoissonSampleParamsData::k_poissonSamples64.data(),
			PoissonSampleParamsData::k_poissonSamples64.size() * sizeof(glm::vec2));

		memcpy(shadowSampleParams.g_poissonSamples32,
			PoissonSampleParamsData::k_poissonSamples32.data(),
			PoissonSampleParamsData::k_poissonSamples32.size() * sizeof(glm::vec2));

		memcpy(shadowSampleParams.g_poissonSamples25,
			PoissonSampleParamsData::k_poissonSamples25.data(),
			PoissonSampleParamsData::k_poissonSamples25.size() * sizeof(glm::vec2));

		return shadowSampleParams;
	}
}