// © 2022 Adam Badke. All rights reserved.
#include "Assert.h"
#include "Camera.h"
#include "Config.h"
#include "Light.h"
#include "MeshFactory.h"
#include "MeshPrimitive.h"
#include "SceneManager.h"
#include "ShadowMap.h"


namespace
{
	float ComputePointLightMeshRadiusScaleFromIntensity(float luminousPower, float emitterRadius, float intensityCutoff)
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
	void Light::ConfigurePointLightMeshScale(fr::Light& light, fr::Transform& transform, fr::Camera* shadowCam)
	{
		SEAssert("Light is not a point light", light.GetType() == fr::Light::LightType::Point_Deferred);

		fr::Light::TypeProperties& lightProperties = 
			light.GetLightTypePropertiesForModification(fr::Light::LightType::Point_Deferred);

		const float newDeferredMeshRadius = ComputePointLightMeshRadiusScaleFromIntensity(
			lightProperties.m_point.m_colorIntensity.a,
			lightProperties.m_point.m_emitterRadius,
			lightProperties.m_point.m_intensityCuttoff);

		// Scale the owning transform such that a sphere created with a radius of 1 will be the correct size
		transform.SetLocalScale(glm::vec3(newDeferredMeshRadius, newDeferredMeshRadius, newDeferredMeshRadius));

		// Update the shadow camera far plane distance using the mesh radius
		if (shadowCam)
		{
			shadowCam->SetNearFar({ 0.1f, newDeferredMeshRadius });
		}
	}


	Light::TypeProperties::TypeProperties()
	{
		memset(this, 0, sizeof(TypeProperties));

		m_type = LightType::LightType_Count;
		m_diffuseEnabled = true;
		m_specularEnabled = true;
	}


	Light::Light(LightType lightType,  glm::vec4 const& colorIntensity)
		: m_isDirty(true)
	{
		m_typeProperties.m_type = lightType;

		switch (lightType)
		{
		case Directional_Deferred:
		{
			m_typeProperties.m_directional.m_colorIntensity = colorIntensity;
		}
		break;
		case Point_Deferred:
		{
			m_typeProperties.m_point.m_colorIntensity = colorIntensity;
			m_typeProperties.m_point.m_emitterRadius = 0.1f;
			m_typeProperties.m_point.m_intensityCuttoff = 0.05f;
		}
		break;
		case AmbientIBL_Deferred:
		{
			SEAssertF("This is the wrong constructor for AmbientIBL_Deferred lights");
		}
		break;
		default: SEAssertF("Invalid light type");
		}
	}


	Light::Light(re::Texture const* iblTex, LightType lightType)
		: m_isDirty(true)
	{
		SEAssert("This constructor is only for AmbientIBL_Deferred lights", lightType == LightType::AmbientIBL_Deferred);

		m_typeProperties.m_type = LightType::AmbientIBL_Deferred;
		m_typeProperties.m_ambient.m_IBLTex = iblTex;
	}


	void Light::Destroy()
	{
		memset(&m_typeProperties, 0, sizeof(m_typeProperties)); // Just zero everything out		
		m_isDirty = true;
	}


	glm::vec4 const& Light::GetColorIntensity() const
	{
		switch (m_typeProperties.m_type)
		{
		case LightType::AmbientIBL_Deferred:
		{
			SEAssertF("Ambient lights don't (current) have a color/intensity value");
		}
		break;
		case LightType::Directional_Deferred:
		{
			return m_typeProperties.m_directional.m_colorIntensity;
		}
		break;
		case LightType::Point_Deferred:
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


	void Light::SetColorIntensity(glm::vec4 const& colorIntensity)
	{
		switch (m_typeProperties.m_type)
		{
		case LightType::AmbientIBL_Deferred:
		{
			SEAssertF("Ambient lights don't (current) have a color/intensity value");
		}
		break;
		case LightType::Directional_Deferred:
		{
			m_typeProperties.m_directional.m_colorIntensity = colorIntensity;
		}
		break;
		case LightType::Point_Deferred:
		{
			m_typeProperties.m_point.m_colorIntensity = colorIntensity;
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}

		m_isDirty = true;
	}


	Light::TypeProperties& Light::GetLightTypePropertiesForModification(fr::Light::LightType lightType)
	{
		SEAssert("Trying to access type properties for the wrong type", lightType == m_typeProperties.m_type);
		m_isDirty = true;
		return m_typeProperties;
	}


	Light::TypeProperties const& Light::GetLightTypeProperties(LightType lightType) const
	{
		SEAssert("Trying to access type properties for the wrong type", lightType == m_typeProperties.m_type);
		return m_typeProperties;
	}


	void Light::ShowImGuiWindow()
	{
		// ECS_CONVERSION: TODO Restore this functionality (move it to the deferred lighting GS)
		// ECS_CONVERSION TODO: Modify the m_isDirty flag here when the settings change
		
		/*
		const uint64_t uniqueID = GetUniqueID();

		auto ShowDebugOptions = [this, &uniqueID]()
		{
			if (ImGui::CollapsingHeader(std::format("Debug##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				ImGui::Checkbox(std::format("Diffuse enabled##{}", uniqueID).c_str(), &m_typeProperties.m_diffuseEnabled);
				ImGui::Checkbox(std::format("Specular enabled##{}", uniqueID).c_str(), &m_typeProperties.m_specularEnabled);
				ImGui::Unindent();
			}
		};

		auto ShowColorPicker = [this, &uniqueID](glm::vec4& color)
		{
			ImGui::Text("Color:"); ImGui::SameLine();
			ImGuiColorEditFlags flags = ImGuiColorEditFlags_HDR;
			ImGui::ColorEdit4(
				std::format("{} Color##{}", GetName(), uniqueID).c_str(),
				&color.r,
				ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | flags);
		};

		auto ShowCommonOptions = [this, &uniqueID, &ShowDebugOptions, &ShowColorPicker](glm::vec4* colorIntensity) -> bool
		{
			const bool currentIsEnabled = m_typeProperties.m_diffuseEnabled || m_typeProperties.m_specularEnabled;

			bool newEnabled = currentIsEnabled;
			ImGui::Checkbox(std::format("Enabled?##{}", uniqueID).c_str(), &newEnabled);
			if (newEnabled != currentIsEnabled)
			{
				m_typeProperties.m_diffuseEnabled = newEnabled;
				m_typeProperties.m_specularEnabled = newEnabled;
			}

			bool modifiedIntensity = false;
			if (colorIntensity)
			{
				modifiedIntensity = ImGui::SliderFloat(
					std::format("Luminous Power##{}", uniqueID).c_str(),
					&colorIntensity->a,
					0.00001f,
					1000.0f,
					"%.3f",
					ImGuiSliderFlags_None);

				ShowColorPicker(*colorIntensity);
			}

			ShowDebugOptions();

			return modifiedIntensity;
		};

		auto ShowShadowMapMenu = [this, &uniqueID](fr::ShadowMap* shadowMap)
		{
			if (ImGui::CollapsingHeader(std::format("Shadow map##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				if (shadowMap)
				{
					shadowMap->ShowImGuiWindow();
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
				transform->ShowImGuiWindow();
				ImGui::Unindent();
			}
		};

		if (ImGui::CollapsingHeader(std::format("{}##{}", GetName(), uniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			switch (m_typeProperties.m_type)
			{
			case LightType::AmbientIBL_Deferred:
			{
				ShowCommonOptions(nullptr);
				
				if (ImGui::CollapsingHeader(std::format("IBL Textures##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
				{
					ImGui::Indent();
					ImGui::Text("BRDF Integration map: \"%s\"",
						m_typeProperties.m_ambient.m_BRDF_integrationMap->GetName().c_str());

					ImGui::Text("IEM Texture: \"%s\"",
						m_typeProperties.m_ambient.m_IEMTex->GetName().c_str());

					ImGui::Text("PMREM Texture: \"%s\"",
						m_typeProperties.m_ambient.m_PMREMTex->GetName().c_str());
					ImGui::Unindent();
				}
			}
			break;
			case LightType::Directional_Deferred:
			{
				ShowCommonOptions(&m_typeProperties.m_directional.m_colorIntensity);

				ShowShadowMapMenu(m_typeProperties.m_directional.m_shadowMap.get());
				ShowTransformMenu(m_typeProperties.m_directional.m_ownerTransform);
			}
			break;
			case LightType::Point_Deferred:
			{
				const bool modifiedIntensity = ShowCommonOptions(&m_typeProperties.m_point.m_colorIntensity);

				const bool modifiedIntensityCutoff = ImGui::SliderFloat(
					std::format("Intensity cutoff##{}", uniqueID).c_str(),
					&m_typeProperties.m_point.m_intensityCuttoff, 0.0f, 1.f, "%.5f", ImGuiSliderFlags_None);

				const bool modifiedEmitterRadius = ImGui::SliderFloat(
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

				if (modifiedIntensity || modifiedIntensityCutoff || modifiedEmitterRadius)
				{
					ConfigurePointLightMeshScale(this);
				}
				ImGui::Text(std::format("Deferred mesh radius: {}", 
					m_typeProperties.m_point.m_ownerTransform->GetLocalScale().x).c_str());

				ShowShadowMapMenu(m_typeProperties.m_point.m_cubeShadowMap.get());
				ShowTransformMenu(m_typeProperties.m_point.m_ownerTransform);
			}
			break;
			default:
				SEAssertF("Invalid light type");
			}

			ImGui::Unindent();
		}	

		*/
	}
}

