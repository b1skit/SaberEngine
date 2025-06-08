// © 2022 Adam Badke. All rights reserved.
#include "Private/Light.h"
#include "Private/ShadowMap.h"
#include "Private/Transform.h"

#include "Core/Assert.h"

#include "Renderer/Texture.h"


namespace
{
	float ConvertLuminousPowerToLuminousIntensity(fr::Light::Type lightType, float luminousPower)
	{
		switch (lightType)
		{
		case fr::Light::Directional:
		{
			SEAssertF("Only punctual lights are (currently) supported");
		}
		break;
		case fr::Light::Point:
		{
			return luminousPower / (4.f * glm::pi<float>());
		}
		break;
		case fr::Light::Spot:
		{
			return luminousPower / glm::pi<float>();
		}
		break;
		case fr::Light::AmbientIBL:
		default: SEAssertF("Invalid light type")
		}
		return 0.f;
	}


	float ComputeLightRadiusFromLuminousPower(
		fr::Light::Type lightType, float luminousPower, float emitterRadius, float intensityCutoff)
	{
		const float luminousIntensity = ConvertLuminousPowerToLuminousIntensity(lightType, luminousPower);

		// In our light shaders, we use Cem Yuksel's nonsingular point light attenuation function:
		// // http://www.cemyuksel.com/research/pointlightattenuation/
		// In the limit over the distance d, it converges to 0 as per the standard 1/d^2 attenuation; In practice it
		// approaches 1/d^2 very quickly. So, we use the simpler 1/d^2 attenuation here to approximate the ideal
		// spherical deffered poiint light mesh radius, as solving for d in Cem's formula has a complex solution

		// See a desmos plot of these calculations here:
		// https://www.desmos.com/calculator/1rtsuljvl4

		const float equivalentConstantOffset = (emitterRadius * emitterRadius) * 0.5f;

		const float minIntensityCutoff = std::max(intensityCutoff, 0.001f); // Guard against divide by 0

		const float deferredMeshRadius = glm::sqrt(std::max(FLT_MIN,
			(luminousIntensity / minIntensityCutoff) - equivalentConstantOffset));

		return deferredMeshRadius;
	}
}


namespace fr
{
	Light::TypeProperties::TypeProperties()
	{
		memset(this, 0, sizeof(TypeProperties));

		m_type = Type::Type_Count;
		m_diffuseEnabled = true;
		m_specularEnabled = true;
	}


	Light::TypeProperties::TypeProperties(TypeProperties const& rhs) noexcept
	{
		*this = rhs;
	}


	Light::TypeProperties::TypeProperties(Light::TypeProperties&& rhs) noexcept
	{
		*this = std::move(rhs);
	}


	Light::TypeProperties& Light::TypeProperties::operator=(Light::TypeProperties const& rhs) noexcept
	{
		if (this != &rhs)
		{
			memset(this, 0, sizeof(TypeProperties));

			m_type = rhs.m_type;

			switch (rhs.m_type)
			{
			case Directional:
			{
				m_directional = rhs.m_directional;
			}
			break;
			case Point:
			{
				m_point = rhs.m_point;
			}
			break;
			case Spot:
			{
				m_spot = rhs.m_spot;
			}
			break;
			case AmbientIBL:
			{
				m_ambient = rhs.m_ambient;
			}
			break;
			default: SEAssertF("Invalid light type");
			}

			m_diffuseEnabled = rhs.m_diffuseEnabled;
			m_specularEnabled = rhs.m_specularEnabled;
		}
		return *this;
	}


	Light::TypeProperties& Light::TypeProperties::operator=(Light::TypeProperties&& rhs) noexcept
	{
		if (this != &rhs)
		{
			// Clean up before we replace anything:
			if (m_type == AmbientIBL)
			{
				m_ambient.m_IBLTex = nullptr; // Don't leak
			}

			// Move:
			m_type = rhs.m_type;
			rhs.m_type = Type::Type_Count;

			switch (m_type)
			{
			case Directional:
			{
				m_directional = rhs.m_directional;
				rhs.m_directional = {};
			}
			break;
			case Point:
			{
				m_point = rhs.m_point;
				rhs.m_point = {};
			}
			break;
			case Spot:
			{
				m_spot = rhs.m_spot;
				rhs.m_spot = {};
			}
			break;
			case AmbientIBL:
			{
				m_ambient = rhs.m_ambient;
				rhs.m_ambient = {};
			}
			break;
			default: SEAssertF("Invalid light type");
			}

			m_diffuseEnabled = rhs.m_diffuseEnabled;
			rhs.m_diffuseEnabled = false;
			
			m_specularEnabled = rhs.m_specularEnabled;
			rhs.m_specularEnabled = false;
		}
		return *this;
	}


	Light::TypeProperties::~TypeProperties()
	{
		switch (m_type)
		{
		case Directional:
		{
			//
		}
		break;
		case Point:
		{
			//
		}
		break;
		case Spot:
		{
			//
		}
		break;
		case AmbientIBL:
		{
			m_ambient.m_IBLTex = nullptr; // Make sure we don't leak
		}
		break;
		default: SEAssertF("Invalid light type");
		}
	}


	Light::Light(Type lightType, glm::vec4 const& colorIntensity)
		: m_isDirty(true)
	{
		m_typeProperties.m_type = lightType;

		switch (lightType)
		{
		case Directional:
		{
			//
		}
		break;
		case Point:
		{
			m_typeProperties.m_point.m_emitterRadius = 0.1f;
			m_typeProperties.m_point.m_intensityCuttoff = 0.1f;
		}
		break;
		case Spot:
		{
			m_typeProperties.m_spot.m_emitterRadius = 0.1f;
			m_typeProperties.m_spot.m_intensityCuttoff = 0.1f;
			m_typeProperties.m_spot.m_innerConeAngle = 0.f;
			m_typeProperties.m_spot.m_outerConeAngle = glm::pi<float>() * 0.25f; // pi/4
		}
		break;
		case AmbientIBL:
		{
			SEAssertF("This is the wrong constructor for AmbientIBL lights");
		}
		break;
		default: SEAssertF("Invalid light type");
		}

		SetColorIntensity(colorIntensity);
	}


	Light::Light(core::InvPtr<re::Texture> const& iblTex, Type lightType)
		: m_isDirty(true)
	{
		SEAssert(lightType == Type::AmbientIBL, "This constructor is only for AmbientIBL lights");

		m_typeProperties.m_type = Type::AmbientIBL;
		m_typeProperties.m_ambient.m_IBLTex = iblTex;
		m_typeProperties.m_ambient.m_diffuseScale = 1.f;
		m_typeProperties.m_ambient.m_specularScale = 1.f;
	}


	glm::vec4 const& Light::GetColorIntensity() const
	{
		switch (m_typeProperties.m_type)
		{
		case Type::AmbientIBL:
		{
			SEAssertF("Ambient lights don't (current) have a color/intensity value");
		}
		break;
		case Type::Directional:
		{
			return m_typeProperties.m_directional.m_colorIntensity;
		}
		break;
		case Type::Point:
		{
			return m_typeProperties.m_point.m_colorIntensity;
		}
		break;
		case Type::Spot:
		{
			return m_typeProperties.m_spot.m_colorIntensity;
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}
		// This shouldn't ever happen, but we need to return something
		return m_typeProperties.m_point.m_colorIntensity;
	}


	bool Light::Update()
	{
		if (!IsDirty())
		{
			return false;
		}

		switch (m_typeProperties.m_type)
		{
		case Type::AmbientIBL:
		{
			//
		}
		break;
		case Type::Directional:
		{
			//
		}
		break;
		case Type::Point:
		{
			// Recompute the spherical radius
			m_typeProperties.m_point.m_sphericalRadius = ComputeLightRadiusFromLuminousPower(
				m_typeProperties.m_type,
				m_typeProperties.m_point.m_colorIntensity.a,
				m_typeProperties.m_point.m_emitterRadius,
				m_typeProperties.m_point.m_intensityCuttoff);
		}
		break;
		case Type::Spot:
		{
			m_typeProperties.m_spot.m_coneHeight = ComputeLightRadiusFromLuminousPower(
				m_typeProperties.m_type,
				m_typeProperties.m_spot.m_colorIntensity.a,
				m_typeProperties.m_spot.m_emitterRadius,
				m_typeProperties.m_spot.m_intensityCuttoff);
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}

		MarkClean();

		return true;
	}


	void Light::SetColorIntensity(glm::vec4 const& colorIntensity)
	{
		switch (m_typeProperties.m_type)
		{
		case Type::AmbientIBL:
		{
			SEAssertF("Ambient lights don't (current) have a color/intensity value");
		}
		break;
		case Type::Directional:
		{
			m_typeProperties.m_directional.m_colorIntensity = colorIntensity;
		}
		break;
		case Type::Point:
		{
			m_typeProperties.m_point.m_colorIntensity = colorIntensity;
		}
		break;
		case Type::Spot:
		{
			m_typeProperties.m_spot.m_colorIntensity = colorIntensity;
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}

		m_isDirty = true;
	}


	Light::TypeProperties const& Light::GetLightTypeProperties(Type lightType) const
	{
		SEAssert(lightType == m_typeProperties.m_type, "Trying to access type properties for the wrong type");
		return m_typeProperties;
	}


	void Light::SetLightTypeProperties(Type lightType, void const* typeProperties)
	{
		SEAssert(typeProperties != nullptr, "Cannot set null type properties");

		switch (lightType)
		{
		case fr::Light::Type::AmbientIBL:
		{
			fr::Light::TypeProperties::AmbientProperties const* properties = 
				static_cast<fr::Light::TypeProperties::AmbientProperties const*>(typeProperties);

			m_typeProperties.m_ambient = *properties;
		}
		break;
		case fr::Light::Type::Directional:
		{
			fr::Light::TypeProperties::DirectionalProperties const* properties =
				static_cast<fr::Light::TypeProperties::DirectionalProperties const*>(typeProperties);
			
			m_typeProperties.m_directional = *properties;
		}
		break;
		case fr::Light::Type::Point:
		{
			fr::Light::TypeProperties::PointProperties const* properties =
				static_cast<fr::Light::TypeProperties::PointProperties const*>(typeProperties);
			
			m_typeProperties.m_point = *properties;
		}
		break;
		case fr::Light::Type::Spot:
		{
			fr::Light::TypeProperties::SpotProperties const* properties =
				static_cast<fr::Light::TypeProperties::SpotProperties const*>(typeProperties);

			SEAssert(properties->m_innerConeAngle >= 0 && properties->m_innerConeAngle < properties->m_outerConeAngle,
				"Invalid inner cone angle. Must be greater than or equal to 0 and less than m_outerConeAngle");

			SEAssert(properties->m_outerConeAngle > properties->m_innerConeAngle && 
				properties->m_outerConeAngle <= glm::pi<float>() * 0.5f,
				"Invalid outer cone angle. Must be greater than m_innerConeAngle and less than or equal to PI / 2");

			m_typeProperties.m_spot = *properties;
		}
		break;
		default: SEAssertF("Invalid type");
		}

		m_isDirty = true;
	}


	void Light::ShowImGuiWindow(uint64_t uniqueID)
	{
		auto ShowDebugOptions = [&]()
		{
			if (ImGui::CollapsingHeader(std::format("Debug##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				m_isDirty |= ImGui::Checkbox(
					std::format("Diffuse enabled##{}", uniqueID).c_str(), &m_typeProperties.m_diffuseEnabled);
				m_isDirty |= ImGui::Checkbox(
					std::format("Specular enabled##{}", uniqueID).c_str(), &m_typeProperties.m_specularEnabled);
				ImGui::Unindent();
			}
		};

		auto ShowColorPicker = [&](glm::vec4& color)
		{
			ImGui::Text("Color:"); ImGui::SameLine();
			ImGuiColorEditFlags flags = ImGuiColorEditFlags_HDR;
			m_isDirty |= ImGui::ColorEdit4(
				std::format("Color##{}", uniqueID).c_str(),
				&color.r,
				ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | flags);
		};

		auto ShowCommonOptions = [this, &uniqueID, &ShowDebugOptions, &ShowColorPicker](glm::vec4* colorIntensity)
		{
			const bool currentIsEnabled = m_typeProperties.m_diffuseEnabled || m_typeProperties.m_specularEnabled;

			bool newEnabled = currentIsEnabled;
			m_isDirty |= ImGui::Checkbox(std::format("Enabled?##{}", uniqueID).c_str(), &newEnabled);
			if (newEnabled != currentIsEnabled)
			{
				m_typeProperties.m_diffuseEnabled = newEnabled;
				m_typeProperties.m_specularEnabled = newEnabled;
			}

			if (colorIntensity)
			{
				m_isDirty |= ImGui::SliderFloat(
					std::format("Luminous Power##{}", uniqueID).c_str(),
					&colorIntensity->a,
					0.00001f,
					1000.0f,
					"%.3f",
					ImGuiSliderFlags_None);

				ShowColorPicker(*colorIntensity);
			}

			ShowDebugOptions();
		};


		switch (m_typeProperties.m_type)
		{
		case Type::AmbientIBL:
		{
			ShowCommonOptions(nullptr);

			if (ImGui::CollapsingHeader(std::format("IBL Texture##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();

				re::Texture::ShowImGuiWindow(m_typeProperties.m_ambient.m_IBLTex);
				
				static bool s_unifyScale = true;
				bool currentUnifyScale = s_unifyScale;
				ImGui::Checkbox("Combine diffuse/specular scale", &s_unifyScale);
				if (s_unifyScale)
				{
					static float s_combinedScale = 1.f;

					// If the checkmark was just toggled, average the 2 values together
					if (currentUnifyScale != s_unifyScale)
					{
						const float avgScale = 
							(m_typeProperties.m_ambient.m_diffuseScale + m_typeProperties.m_ambient.m_specularScale) * 0.5f;
						
						m_typeProperties.m_ambient.m_diffuseScale = avgScale;
						m_typeProperties.m_ambient.m_specularScale = avgScale;
						s_combinedScale = avgScale;

						m_isDirty = true;
					}
					
					if (ImGui::SliderFloat("Intensity scale", &s_combinedScale, 0.f, 10.f))
					{
						m_typeProperties.m_ambient.m_diffuseScale = s_combinedScale;
						m_typeProperties.m_ambient.m_specularScale = s_combinedScale;
						m_isDirty = true;
					}
				}
				else
				{
					if (!m_typeProperties.m_diffuseEnabled)
					{
						ImGui::BeginDisabled();
					}
					m_isDirty |= ImGui::SliderFloat("Diffuse scale", &m_typeProperties.m_ambient.m_diffuseScale, 0.f, 10.f);
					if (!m_typeProperties.m_diffuseEnabled)
					{
						ImGui::EndDisabled();
					}

					if (!m_typeProperties.m_specularEnabled)
					{
						ImGui::BeginDisabled();
					}
					m_isDirty |= ImGui::SliderFloat("Specular scale", &m_typeProperties.m_ambient.m_specularScale, 0.f, 10.f);
					if (!m_typeProperties.m_specularEnabled)
					{
						ImGui::EndDisabled();
					}
				}				
				
				ImGui::Unindent();
			}
		}
		break;
		case Type::Directional:
		{
			ShowCommonOptions(&m_typeProperties.m_directional.m_colorIntensity);
		}
		break;
		case Type::Point:
		{
			ShowCommonOptions(&m_typeProperties.m_point.m_colorIntensity);

			m_isDirty |= ImGui::SliderFloat(
				std::format("Intensity cutoff##{}", uniqueID).c_str(),
				&m_typeProperties.m_point.m_intensityCuttoff, 0.0f, 1.f, "%.5f", ImGuiSliderFlags_None);

			m_isDirty |= ImGui::SliderFloat(
				std::format("Emitter Radius##{}", uniqueID).c_str(),
				&m_typeProperties.m_point.m_emitterRadius, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_None);
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::BeginItemTooltip())
			{
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::TextUnformatted("Simulated emitter radius for calculating non-singular point light attenuation");
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}

			ImGui::Text(
				std::format("Deferred mesh radius: {}##{}", m_typeProperties.m_point.m_sphericalRadius, uniqueID).c_str());
		}
		break;
		case Type::Spot:
		{
			ShowCommonOptions(&m_typeProperties.m_spot.m_colorIntensity);

			constexpr ImGuiSliderFlags k_flags =
				ImGuiSliderFlags_::ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_NoRoundToFormat;

			m_isDirty |= ImGui::SliderFloat(
				std::format("Inner cone angle##{}", uniqueID).c_str(),
				&m_typeProperties.m_spot.m_innerConeAngle,
				0.f,
				m_typeProperties.m_spot.m_outerConeAngle - FLT_MIN,
				nullptr,
				k_flags);

			m_isDirty |= ImGui::SliderFloat(
				std::format("Outer cone angle##{}", uniqueID).c_str(),
				&m_typeProperties.m_spot.m_outerConeAngle,
				m_typeProperties.m_spot.m_innerConeAngle + FLT_MIN,
				glm::pi<float>() * 0.5f,
				nullptr,
				k_flags);

			m_isDirty |= ImGui::SliderFloat(
				std::format("Intensity cutoff##{}", uniqueID).c_str(),
				&m_typeProperties.m_spot.m_intensityCuttoff, 0.0f, 1.f, "%.5f", ImGuiSliderFlags_None);

			m_isDirty |= ImGui::SliderFloat(
				std::format("Emitter Radius##{}", uniqueID).c_str(),
				&m_typeProperties.m_spot.m_emitterRadius, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_None);
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
			if (ImGui::BeginItemTooltip())
			{
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::TextUnformatted("Simulated emitter radius for calculating non-singular spot light attenuation");
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}

			ImGui::Text(
				std::format("Deferred mesh height: {}##{}", m_typeProperties.m_spot.m_coneHeight, uniqueID).c_str());
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}
	}
}

