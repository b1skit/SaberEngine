// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "Core/Config.h"
#include "Core/Util/ImGuiUtils.h"
#include "ShadowMap.h"


namespace
{
	fr::ShadowMap::ShadowType GetShadowTypeFromLightType(fr::Light::Type lightType)
	{
		switch (lightType)
		{
		case fr::Light::Type::Directional: return fr::ShadowMap::ShadowType::Orthographic;
		case fr::Light::Type::Point: return fr::ShadowMap::ShadowType::CubeMap;
		case fr::Light::Type::Spot: return fr::ShadowMap::ShadowType::Perspective;
		case fr::Light::Type::AmbientIBL:
		default:
			SEAssertF("Invalid or unsupported light type for shadow map");
		}
		return fr::ShadowMap::ShadowType::ShadowType_Count;
	}


	void SetDefaults(fr::ShadowMap::ShadowParams& shadowParams)
	{
		switch (shadowParams.m_lightType)
		{
		case fr::Light::Type::Directional:
		{
			shadowParams.m_shadowQuality = fr::ShadowMap::ShadowQuality::PCSS_HIGH;

			shadowParams.m_minMaxShadowBias = glm::vec2(
				core::Config::Get()->GetValue<float>(core::configkeys::k_defaultDirectionalLightMinShadowBiasKey),
				core::Config::Get()->GetValue<float>(core::configkeys::k_defaultDirectionalLightMaxShadowBiasKey));

			shadowParams.m_softness = core::Config::Get()->GetValue<float>(core::configkeys::k_defaultDirectionalLightShadowSoftnessKey);

			shadowParams.m_orthographic.m_frustumSnapMode = fr::ShadowMap::ShadowParams::Orthographic::ActiveCamera;
		}
		break;
		case fr::Light::Type::Spot:
		{
			shadowParams.m_shadowQuality = fr::ShadowMap::ShadowQuality::PCSS_HIGH;

			shadowParams.m_minMaxShadowBias = glm::vec2(
				core::Config::Get()->GetValue<float>(core::configkeys::k_defaultSpotLightMinShadowBiasKey),
				core::Config::Get()->GetValue<float>(core::configkeys::k_defaultSpotLightMaxShadowBiasKey));

			shadowParams.m_softness = core::Config::Get()->GetValue<float>(core::configkeys::k_defaultSpotLightShadowSoftnessKey);
		}
		break;
		case fr::Light::Type::Point:
		{
			shadowParams.m_shadowQuality = fr::ShadowMap::ShadowQuality::PCSS_HIGH;

			shadowParams.m_minMaxShadowBias = glm::vec2(
				core::Config::Get()->GetValue<float>(core::configkeys::k_defaultPointLightMinShadowBiasKey),
				core::Config::Get()->GetValue<float>(core::configkeys::k_defaultPointLightMaxShadowBiasKey));

			shadowParams.m_softness = core::Config::Get()->GetValue<float>(core::configkeys::k_defaultPointLightShadowSoftnessKey);
		}
		break;
		case fr::Light::Type::AmbientIBL:
		default: SEAssertF("Invalid light type");
		}
	}
}

namespace fr
{
	ShadowMap::ShadowMap(fr::Light::Type lightType)
		: m_typeProperties{ 
			.m_shadowType = GetShadowTypeFromLightType(lightType), 
			.m_lightType = lightType,
			.m_shadowQuality = ShadowQuality::ShadowQuality_Count,
			.m_minMaxShadowBias = glm::vec2(0.f, 0.f), }
		, m_isEnabled(true)
		, m_isDirty(true)
	{
		SetDefaults(m_typeProperties);
	}


	void ShadowMap::SetMinMaxShadowBias(glm::vec2 const& minMaxShadowBias)
	{
		m_typeProperties.m_minMaxShadowBias = minMaxShadowBias;
		m_isDirty = true;
	}


	void ShadowMap::ShowImGuiWindow(uint64_t uniqueID)
	{
		m_isDirty |= ImGui::Checkbox(std::format("Shadow enabled?##{}", uniqueID).c_str(), &m_isEnabled);

		constexpr char const* k_qualityNames[] = { "PCS", "PCSS Low", "PCSS High" };
		static int currentQuality = static_cast<int>(m_typeProperties.m_shadowQuality);
		if (ImGui::Combo(
			std::format("Quality##{}", uniqueID).c_str(), &currentQuality, k_qualityNames, IM_ARRAYSIZE(k_qualityNames)))
		{
			m_isDirty = true;
			m_typeProperties.m_shadowQuality = static_cast<ShadowMap::ShadowQuality>(currentQuality);
		}

		const bool softnessIsSelectable = 
			m_typeProperties.m_shadowQuality == ShadowQuality::PCSS_LOW || 
			m_typeProperties.m_shadowQuality == ShadowQuality::PCSS_HIGH;

		ImGui::BeginDisabled(!softnessIsSelectable);
		m_isDirty |= ImGui::SliderFloat(std::format("Softness##{}", uniqueID).c_str(), &m_typeProperties.m_softness, 0.f, 1.f);
		ImGui::SetItemTooltip("PCSS light size");
		ImGui::EndDisabled();

		m_isDirty |= ImGui::SliderFloat(
			std::format("Min shadow bias##{}", uniqueID).c_str(), 
			&m_typeProperties.m_minMaxShadowBias.x, 
			0.f, 
			0.1f, 
			"%.5f");
		
		m_isDirty |= ImGui::SliderFloat(
			std::format("Max shadow bias##{}", uniqueID).c_str(),
			&m_typeProperties.m_minMaxShadowBias.y,
			0.f,
			0.1f,
			"%.5f");

		if (ImGui::Button(std::format("Reset##{}", uniqueID).c_str()))
		{
			SetDefaults(m_typeProperties);
			
			m_isDirty = true;
		}

		// Type-specific settings:
		switch (m_typeProperties.m_shadowType)
		{
		case ShadowType::Orthographic:
		{
			m_isDirty |= util::ShowBasicComboBox("Shadow camera snap mode",
				ShadowMap::ShadowParams::Orthographic::k_frustumSnapModeNames.data(),
				ShadowMap::ShadowParams::Orthographic::k_frustumSnapModeNames.size(),
				m_typeProperties.m_orthographic.m_frustumSnapMode);
		}
		break;
		case ShadowType::Perspective:
		{
			//
		}
		break;
		case ShadowType::CubeMap:
		{
			//
		}
		break;
		default: SEAssertF("Invalid shadow type");
		}
	}
}


