// � 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "Config.h"
#include "ShadowMap.h"


namespace
{
	fr::ShadowMap::ShadowType GetShadowTypeFromLightType(fr::Light::Type lightType)
	{
		switch (lightType)
		{
		case fr::Light::Type::Directional: return fr::ShadowMap::ShadowType::Orthographic;
		case fr::Light::Type::Point: return fr::ShadowMap::ShadowType::CubeMap;
		case fr::Light::Type::AmbientIBL:
		default:
			SEAssertF("Invalid or unsupported light type for shadow map");
		}
		return fr::ShadowMap::ShadowType::ShadowType_Count;
	}
}

namespace fr
{
	ShadowMap::ShadowMap(glm::uvec2 widthHeight, fr::Light::Type lightType)
		: m_shadowType(GetShadowTypeFromLightType(lightType))
		, m_lightType(lightType)
		, m_shadowQuality(ShadowQuality::ShadowQuality_Count)
		, m_widthHeight(widthHeight)
		, m_minMaxShadowBias(0.f, 0.f)
		, m_isEnabled(true)
		, m_isDirty(true)
	{
		switch (m_shadowType)
		{
		case ShadowType::CubeMap:
		{
			m_shadowQuality = ShadowQuality::PCSS_HIGH;

			m_minMaxShadowBias = glm::vec2(
				en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMinShadowBias),
				en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMaxShadowBias));

			m_softness = en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightShadowSoftness);
		}
		break;
		case ShadowType::Orthographic:
		{
			m_shadowQuality = ShadowQuality::PCSS_HIGH;

			m_minMaxShadowBias = glm::vec2(
				en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMinShadowBias),
				en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias));

			m_softness = en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightShadowSoftness);
		}
		break;
		default: SEAssertF("Invalid ShadowType");
		}
	}


	void ShadowMap::SetMinMaxShadowBias(glm::vec2 const& minMaxShadowBias)
	{
		m_minMaxShadowBias = minMaxShadowBias;
		m_isDirty = true;
	}


	void ShadowMap::ShowImGuiWindow(uint64_t uniqueID)
	{
		m_isDirty |= ImGui::Checkbox(std::format("Shadow enabled?##{}", uniqueID).c_str(), &m_isEnabled);

		const char* qualityNames[] = { "PCS", "PCSS Low", "PCSS High" };
		static int currentQuality = static_cast<int>(m_shadowQuality);
		if (ImGui::Combo(
			std::format("Quality##{}", uniqueID).c_str(), &currentQuality, qualityNames, IM_ARRAYSIZE(qualityNames)))
		{
			m_isDirty = true;
			m_shadowQuality = static_cast<ShadowMap::ShadowQuality>(currentQuality);
		}

		bool softnessIsSelectable = 
			m_shadowQuality == ShadowQuality::PCSS_LOW || m_shadowQuality == ShadowQuality::PCSS_HIGH;
		ImGui::BeginDisabled(!softnessIsSelectable);
		m_isDirty |= ImGui::SliderFloat(std::format("Softness##{}", uniqueID).c_str(), &m_softness, 0.f, 1.f);
		ImGui::SetItemTooltip("PCSS light size");
		ImGui::EndDisabled();

		std::string const& minLabel = std::format("Min shadow bias##{}", uniqueID);
		m_isDirty |= ImGui::SliderFloat(minLabel.c_str(), &m_minMaxShadowBias.x, 0.f, 0.1f, "%.5f");
		
		std::string const& maxLabel = std::format("Max shadow bias##{}", uniqueID);
		m_isDirty |= ImGui::SliderFloat(maxLabel.c_str(), &m_minMaxShadowBias.y, 0.f, 0.1f, "%.5f");

		std::string const& resetLabel = std::format("Reset biases to defaults##{}", uniqueID);
		if (ImGui::Button(resetLabel.c_str()))
		{
			switch (m_lightType)
			{
			case fr::Light::Type::Directional:
			{
				m_minMaxShadowBias = glm::vec2(
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMinShadowBias),
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias));
			}
			break;
			case fr::Light::Type::Point:
			{
				m_minMaxShadowBias = glm::vec2(
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMinShadowBias),
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMaxShadowBias));
			}
			break;
			default: SEAssertF("Invalid/unsupported light type");
			}
			m_isDirty = true;
		}
	}
}


