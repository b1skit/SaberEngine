// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Light.h"
#include "ShadowMap.h"
#include "Texture.h"
#include "Transform.h"


namespace
{
	float ComputePointLightRadiusFromIntensity(float luminousPower, float emitterRadius, float intensityCutoff)
	{
		// As per equation 15 (p.29) of "Moving Frostbite to Physically Based Rendering 3.0", Lagarde et al, we can 
		// convert a point light's luminous power I (lm) (a.k.a luminous flux: The *perceived* power of a light, with 
		// respect to the sensitivity of the human eye) to luminous intensity (phi) (the quantity of light emitted in
		// unit time per unit solid angle) using phi = 4pi * I. 
		// Intuitively, this conversion is taking the perceived intensity of a ray arriving at the eye, and adjusting
		// it with respect to all of the rays emitted by a spherical emitter.
		// However, for our point light approximation we're evaluating the luminous power arriving from a signel ray
		// (not integrated over a spherical emitter) so we normalize it over the solid angle by dividing by 4pi. Thus,
		// the 4pi's cancel and we can ignore them here
		const float luminousIntensity = luminousPower;

		// In our point light shaders, we use Cem Yuksel's nonsingular point light attenuation function:
		// // http://www.cemyuksel.com/research/pointlightattenuation/
		// In the limit over the distance d, it converges to 0 as per the standard 1/d^2 attenuation; In practice it
		// approaches 1/d^2 very quickly. So, we use the simpler 1/d^2 attenuation here to approximate the ideal
		// spherical deffered poiint light mesh radius, as solving for d in Cem's formula has a complex solution

		// See a desmos plot of these calculations here:
		// https://www.desmos.com/calculator/1rtsuljvl4

		const float equivalentConstantOffset = (emitterRadius * emitterRadius) * 0.5f;

		const float minIntensityCutoff = std::max(intensityCutoff, 0.001f); // Guard against divide by 0

		const float deferredMeshRadius = glm::sqrt((luminousIntensity / minIntensityCutoff) - equivalentConstantOffset);

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


	Light::Light(Type lightType,  glm::vec4 const& colorIntensity)
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
			m_typeProperties.m_point.m_intensityCuttoff = 0.05f;
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


	Light::Light(re::Texture const* iblTex, Type lightType)
		: m_isDirty(true)
	{
		SEAssert("This constructor is only for AmbientIBL lights", lightType == Type::AmbientIBL);

		m_typeProperties.m_type = Type::AmbientIBL;
		m_typeProperties.m_ambient.m_IBLTex = iblTex;
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
			m_typeProperties.m_point.m_sphericalRadius = ComputePointLightRadiusFromIntensity(
				m_typeProperties.m_point.m_colorIntensity.a,
				m_typeProperties.m_point.m_emitterRadius,
				m_typeProperties.m_point.m_intensityCuttoff);
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

			m_typeProperties.m_point.m_sphericalRadius = ComputePointLightRadiusFromIntensity(
				m_typeProperties.m_point.m_colorIntensity.a,
				m_typeProperties.m_point.m_emitterRadius,
				m_typeProperties.m_point.m_intensityCuttoff);
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}

		m_isDirty = true;
	}


	Light::TypeProperties& Light::GetLightTypePropertiesForModification(fr::Light::Type lightType)
	{
		SEAssert("Trying to access type properties for the wrong type", lightType == m_typeProperties.m_type);
		m_isDirty = true;
		return m_typeProperties;
	}


	Light::TypeProperties const& Light::GetLightTypeProperties(Type lightType) const
	{
		SEAssert("Trying to access type properties for the wrong type", lightType == m_typeProperties.m_type);
		return m_typeProperties;
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

		auto ShowShadowMapMenu = [this, &uniqueID](fr::ShadowMap* shadowMap)
		{
			if (ImGui::CollapsingHeader(std::format("Shadow map##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				if (shadowMap)
				{
					shadowMap->ShowImGuiWindow(uniqueID);
				}
				else
				{
					ImGui::Text("<No Shadow>");
				}
				ImGui::Unindent();
			}
		};

		auto ShowTransformMenu = [this, &uniqueID](fr::Transform* transform)
		{
			if (ImGui::CollapsingHeader(std::format("Transform##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				transform->ShowImGuiWindow(uniqueID);
				ImGui::Unindent();
			}
		};

		switch (m_typeProperties.m_type)
		{
		case Type::AmbientIBL:
		{
			ShowCommonOptions(nullptr);

			if (ImGui::CollapsingHeader(std::format("IBL Textures##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				ImGui::Text("IBL texture: \"%s\"", m_typeProperties.m_ambient.m_IBLTex->GetName().c_str());
				ImGui::Unindent();
			}
		}
		break;
		case Type::Directional:
		{
			ShowCommonOptions(&m_typeProperties.m_directional.m_colorIntensity);

			//ShowShadowMapMenu(shadowMap);
			//ShowTransformMenu(owningTransform);
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

			//ShowShadowMapMenu(shadowMap);
			//ShowTransformMenu(owningTransform);
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}
	}
}

