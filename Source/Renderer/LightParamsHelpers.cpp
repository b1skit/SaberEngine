// © 2024 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "LightParamsHelpers.h"
#include "LightRenderData.h"
#include "RenderDataManager.h"

#include "Texture.h"
#include "TransformRenderData.h"

#include "Core/Config.h"
#include "Core/InvPtr.h"

#include "Renderer/Shaders/Common/LightParams.h"
#include "Renderer/Shaders/Common/ShadowParams.h"


namespace
{
	LightData CreateLightDataHelper(
		void const* lightRenderData,
		gr::Light::Type lightType,
		gr::RenderDataID lightID,
		gr::RenderDataManager const& renderData)
	{
		SEAssert(lightType != gr::Light::Type::IBL,
			"Ambient lights do not use the LightData structure");

		gr::Transform::RenderData const& transformData = renderData.GetTransformDataFromRenderDataID(lightID);

		LightData lightData{};

		// Packed below as we go
		bool diffuseEnabled = false;
		bool specEnabled = false;
		glm::vec4 intensityScale(0.f);
		glm::vec4 extraParams(0.f);

		switch (lightType)
		{
		case gr::Light::Type::Directional:
		{
			gr::Light::RenderDataDirectional const* directionalData =
				static_cast<gr::Light::RenderDataDirectional const*>(lightRenderData);

			lightData.g_lightColorIntensity = directionalData->m_colorIntensity;

			// As per the KHR_lights_punctual, directional lights are at infinity and emit light in the direction of the
			// local -Z axis. Thus, this direction is pointing towards the source of the light (saves a * -1 on the GPU)
			// https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#directional
			lightData.g_lightWorldPosRadius = glm::vec4(transformData.m_globalForward, 0.f); // WorldPos == Dir to light

			diffuseEnabled = directionalData->m_diffuseEnabled;
			specEnabled = directionalData->m_specularEnabled;
		}
		break;
		case gr::Light::Type::Point:
		{
			gr::Light::RenderDataPoint const* pointData =
				static_cast<gr::Light::RenderDataPoint const*>(lightRenderData);

			lightData.g_lightColorIntensity = pointData->m_colorIntensity;

			lightData.g_lightWorldPosRadius = glm::vec4(transformData.m_globalPosition, pointData->m_emitterRadius);

			diffuseEnabled = pointData->m_diffuseEnabled;
			specEnabled = pointData->m_specularEnabled;
		}
		break;
		case gr::Light::Type::Spot:
		{
			gr::Light::RenderDataSpot const* spotData = static_cast<gr::Light::RenderDataSpot const*>(lightRenderData);

			lightData.g_lightColorIntensity = spotData->m_colorIntensity;

			lightData.g_lightWorldPosRadius = glm::vec4(transformData.m_globalPosition, spotData->m_emitterRadius);

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

		// Direction the light is emitting from the light source. SE uses a RHCS, so this is the local -Z direction
		lightData.g_globalForwardDir = glm::vec4(transformData.m_globalForward * -1.f, 0.f);

		lightData.g_intensityScale = intensityScale;

		lightData.g_extraParams = extraParams;

		return lightData;
	}
}

namespace grutil
{
	AmbientLightData GetAmbientLightData(
		uint32_t numPMREMMips,
		float diffuseScale, 
		float specularScale, 
		const uint32_t dfgTexWidthHeight, 
		core::InvPtr<re::Texture> const& AOTex)
	{

		AmbientLightData ambientLightParamsData{};

		assert(numPMREMMips > 0); // Don't underflow!
		const uint32_t maxPMREMMipLevel = numPMREMMips - 1;

		ambientLightParamsData.g_maxPMREMMipDFGResScaleDiffuseScaleSpec = glm::vec4(
			maxPMREMMipLevel,
			dfgTexWidthHeight,
			diffuseScale,
			specularScale);

		ambientLightParamsData.g_AOTexDims = glm::vec4(0.f);
		if (AOTex)
		{
			ambientLightParamsData.g_AOTexDims = AOTex->GetTextureDimenions();
		}

		return ambientLightParamsData;
	}


	LightData CreateDirectionalLightData(
		gr::Light::RenderDataDirectional const& lightRenderData,
		gr::IDType lightID,
		gr::RenderDataManager const& renderData)
	{
		return CreateLightDataHelper(&lightRenderData, gr::Light::Directional, lightID, renderData);
	}


	LightData CreatePointLightData(
		gr::Light::RenderDataPoint const& lightRenderData,
		gr::IDType lightID,
		gr::RenderDataManager const& renderData)
	{
		return CreateLightDataHelper(&lightRenderData, gr::Light::Point, lightID, renderData);
	}


	LightData CreateSpotLightData(
		gr::Light::RenderDataSpot const& lightRenderData,
		gr::IDType lightID,
		gr::RenderDataManager const& renderData)
	{
		return CreateLightDataHelper(&lightRenderData, gr::Light::Spot, lightID, renderData);
	}


	ShadowData CreateShadowData(
		gr::ShadowMap::RenderData const& shadowRenderData,
		gr::IDType lightRenderDataID,
		gr::RenderDataManager const& renderData)
	{
		gr::Camera::RenderData const& shadowCamRenderData =
			renderData.GetObjectData<gr::Camera::RenderData>(lightRenderDataID);

		bool usesShadowCamVP = true;
		glm::vec4 shadowMapTexelSize;
		switch (shadowRenderData.m_lightType)
		{
		case gr::Light::Type::Directional:
		{
			const int defaultDirectionalWidthHeight =
				core::Config::GetValue<int>(core::configkeys::k_defaultDirectionalShadowMapResolutionKey);

			shadowMapTexelSize = glm::vec4(
				defaultDirectionalWidthHeight,
				defaultDirectionalWidthHeight,
				1.f / defaultDirectionalWidthHeight,
				1.f / defaultDirectionalWidthHeight);
		}
		break;
		case gr::Light::Type::Point:
		{
			const int defaultPointWidthHeight =
				core::Config::GetValue<int>(core::configkeys::k_defaultShadowCubeMapResolutionKey);

			shadowMapTexelSize = glm::vec4(
				defaultPointWidthHeight,
				defaultPointWidthHeight,
				1.f / defaultPointWidthHeight,
				1.f / defaultPointWidthHeight);

			usesShadowCamVP = false;
		}
		break;
		case gr::Light::Type::Spot:
		{
			const int defaultSpotWidthHeight =
				core::Config::GetValue<int>(core::configkeys::k_defaultSpotShadowMapResolutionKey);

			shadowMapTexelSize = glm::vec4(
				defaultSpotWidthHeight,
				defaultSpotWidthHeight,
				1.f / defaultSpotWidthHeight,
				1.f / defaultSpotWidthHeight);
		}
		break;
		default: SEAssertF("Invalid light type for ShadowData");
		}

		return ShadowData {
			.g_shadowCam_VP = usesShadowCamVP ? 
				shadowCamRenderData.m_cameraParams.g_viewProjection : glm::mat4(0.f), // Unused by point lights
			.g_shadowMapTexelSize = shadowMapTexelSize,
			.g_shadowCamNearFarBiasMinMax = glm::vec4(
				shadowCamRenderData.m_cameraConfig.m_near,
				shadowCamRenderData.m_cameraConfig.m_far,
				shadowRenderData.m_minMaxShadowBias),
			.g_shadowParams = glm::vec4(
				shadowRenderData.m_shadowEnabled,
				static_cast<float>(shadowRenderData.m_shadowQuality),
				shadowRenderData.m_softness, // [0,1] uv radius X
				shadowRenderData.m_softness),
		};
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