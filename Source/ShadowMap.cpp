// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "Config.h"
#include "ShadowMap.h"


namespace
{
	fr::ShadowMap::ShadowType GetShadowTypeFromLightType(fr::Light::LightType lightType)
	{
		switch (lightType)
		{
		case fr::Light::LightType::Directional_Deferred: return fr::ShadowMap::ShadowType::Orthographic;
		case fr::Light::LightType::Point_Deferred: return fr::ShadowMap::ShadowType::CubeMap;
		case fr::Light::LightType::AmbientIBL_Deferred:
		default:
			SEAssertF("Invalid or unsupported light type for shadow map");
		}
		return fr::ShadowMap::ShadowType::ShadowType_Count;
	}
}

namespace fr
{
	ShadowMap::ShadowMap(glm::uvec2 widthHeight, fr::Light::LightType lightType)
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


	void ShadowMap::ShowImGuiWindow()
	{
		// ECS_CONVERSION TODO: Restore the functionality
		// -> Modify the m_isDirty flag here when the settings change
		/*
		ImGui::Text("Name: \"%s\"", GetName().c_str());

		const std::string minLabel = "Min shadow bias##" + GetName();
		ImGui::SliderFloat(minLabel.c_str(), &m_minMaxShadowBias.x, 0, 0.1f, "%.5f");
		
		const std::string maxLabel = "Max shadow bias##" + GetName();
		ImGui::SliderFloat(maxLabel.c_str(), &m_minMaxShadowBias.y, 0, 0.1f, "%.5f");

		const std::string resetLabel = "Reset biases to defaults##" + GetName();
		if (ImGui::Button(resetLabel.c_str()))
		{
			switch (m_lightType)
			{
			case fr::Light::LightType::Directional_Deferred:
			{
				m_minMaxShadowBias = glm::vec2(
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMinShadowBias),
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias));
			}
			break;
			case fr::Light::LightType::Point_Deferred:
			{
				m_minMaxShadowBias = glm::vec2(
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMinShadowBias),
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMaxShadowBias));
			}
			break;
			default: SEAssertF("Invalid/unsupported light type");
			}
		}

		if (ImGui::CollapsingHeader("Shadow Map Camera", ImGuiTreeNodeFlags_None))
		{
			m_shadowCam->ShowImGuiWindow();
		}

		*/
	}
}


