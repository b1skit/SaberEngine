// © 2022 Adam Badke. All rights reserved.
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
		, m_widthHeight(widthHeight)
		, m_isDirty(true)
	{	
		switch (m_shadowType)
		{
		case ShadowType::CubeMap:
		{
			m_minMaxShadowBias = glm::vec2(
				en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMinShadowBias),
				en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMaxShadowBias));
		}
		break;
		case ShadowType::Orthographic:
		{
			m_minMaxShadowBias = glm::vec2(
				en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMinShadowBias),
				en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias));
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
		std::string const& minLabel = std::format("Min shadow bias##{}", uniqueID);
		m_isDirty |= ImGui::SliderFloat(minLabel.c_str(), &m_minMaxShadowBias.x, 0.f, 1.f, "%.5f");
		
		std::string const& maxLabel = std::format("Max shadow bias##{}", uniqueID);
		m_isDirty |= ImGui::SliderFloat(maxLabel.c_str(), &m_minMaxShadowBias.y, 0.f, 1.f, "%.5f");

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


