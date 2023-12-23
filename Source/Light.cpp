// � 2022 Adam Badke. All rights reserved.
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


	void ConfigurePointLightMeshScale(fr::Light* pointLight)
	{
		SEAssert("Light is not a point light", pointLight->GetType() == fr::Light::LightType::Point_Deferred);

		fr::Light::TypeProperties& lightProperties = 
			pointLight->AccessLightTypeProperties(fr::Light::LightType::Point_Deferred);

		const float newDeferredMeshRadius = ComputePointLightMeshRadiusScaleFromIntensity(
			lightProperties.m_point.m_colorIntensity.a,
			lightProperties.m_point.m_emitterRadius,
			lightProperties.m_point.m_intensityCuttoff);

		// Scale the owning transform such that a sphere created with a radius of 1 will be the correct size
		lightProperties.m_point.m_ownerTransform->SetLocalScale(
			glm::vec3(newDeferredMeshRadius, newDeferredMeshRadius, newDeferredMeshRadius));
	}
}


namespace fr
{
	std::shared_ptr<fr::Light> Light::CreateAmbientLight(std::string const& name)
	{
		std::shared_ptr<fr::Light> newAmbientLight;
		newAmbientLight.reset(new fr::Light(name, nullptr, Light::AmbientIBL_Deferred, glm::vec4(1.f, 0.f, 1.f, 1.f), false));
		en::SceneManager::GetSceneData()->AddLight(newAmbientLight);
		return newAmbientLight;
	}


	std::shared_ptr<fr::Light> Light::CreateDirectionalLight(
		std::string const& name, fr::Transform* ownerTransform, glm::vec4 colorIntensity, bool hasShadow)
	{
		std::shared_ptr<fr::Light> newDirectionalLight;
		newDirectionalLight.reset(new fr::Light(name, ownerTransform, Light::Directional_Deferred, colorIntensity, hasShadow));
		en::SceneManager::GetSceneData()->AddLight(newDirectionalLight);
		return newDirectionalLight;
	}


	std::shared_ptr<fr::Light> Light::CreatePointLight(
		std::string const& name, fr::Transform* ownerTransform, glm::vec4 colorIntensity, bool hasShadow)
	{
		std::shared_ptr<fr::Light> newPointLight;
		newPointLight.reset(new fr::Light(name, ownerTransform, Light::Point_Deferred, colorIntensity, hasShadow));
		en::SceneManager::GetSceneData()->AddLight(newPointLight);
		return newPointLight;
	}


	Light::TypeProperties::TypeProperties()
	{
		memset(this, 0, sizeof(TypeProperties));

		m_type = LightType::LightType_Count;
		m_diffuseEnabled = true;
		m_specularEnabled = true;
	}


	Light::TypeProperties::~TypeProperties()
	{
		switch (m_type)
		{
		case fr::Light::LightType::AmbientIBL_Deferred:
		{
			m_ambient.m_IBLTex = nullptr;
		}
		break;
		case fr::Light::LightType::Directional_Deferred:
		{
			m_directional.m_ownerTransform = nullptr;
			m_directional.m_shadowMap = nullptr;
			m_directional.m_screenAlignedQuad = nullptr;
		}
		break;
		case fr::Light::LightType::Point_Deferred:
		{
			m_point.m_ownerTransform = nullptr;
			m_point.m_sphereMeshPrimitive = nullptr;
			m_point.m_cubeShadowMap = nullptr;
		}
		break;
		default: SEAssertF("Invalid light type");
		}
	}


	Light::Light(
		std::string const& name, fr::Transform* ownerTransform, LightType lightType, glm::vec4 colorIntensity, bool hasShadow)
		: en::NamedObject(name)
	{
		m_typeProperties.m_type = lightType;

		switch (lightType)
		{
		case Directional_Deferred:
		{
			m_typeProperties.m_directional.m_ownerTransform = ownerTransform;
			m_typeProperties.m_directional.m_colorIntensity = colorIntensity;

			m_typeProperties.m_directional.m_screenAlignedQuad = 
				gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Far);

			m_typeProperties.m_directional.m_shadowMap = nullptr;
			if (hasShadow)
			{
				const uint32_t shadowMapRes = en::Config::Get()->GetValue<int>("defaultShadowMapRes");
				m_typeProperties.m_directional.m_shadowMap = std::make_unique<fr::ShadowMap>(
					GetName(),
					shadowMapRes,
					shadowMapRes,
					m_typeProperties.m_directional.m_ownerTransform,
					m_typeProperties.m_type,
					ownerTransform);
				// Note: We'll compute the camera config from the scene bounds during the first call to Update(); so
				// here we just pass a default camera config

				m_typeProperties.m_directional.m_shadowMap->SetMinMaxShadowBias(glm::vec2(
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMinShadowBias),
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias)));
			}
		}
		break;
		case Point_Deferred:
		{
			m_typeProperties.m_point.m_ownerTransform = ownerTransform;
			m_typeProperties.m_point.m_colorIntensity = colorIntensity;

			m_typeProperties.m_point.m_emitterRadius = 0.1f;
			m_typeProperties.m_point.m_intensityCuttoff = 0.05f;

			ConfigurePointLightMeshScale(this);
			
			const float deferredMeshRadius = m_typeProperties.m_point.m_ownerTransform->GetLocalScale().x;

			m_typeProperties.m_point.m_sphereMeshPrimitive = gr::meshfactory::CreateSphere(1.0f);

			m_typeProperties.m_point.m_cubeShadowMap = nullptr;
			if (hasShadow)
			{
				const uint32_t cubeMapRes = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_defaultShadowMapResolution);

				m_typeProperties.m_point.m_cubeShadowMap = make_unique<fr::ShadowMap>(
					GetName(),
					cubeMapRes,
					cubeMapRes,
					m_typeProperties.m_point.m_ownerTransform,
					m_typeProperties.m_type,
					ownerTransform);

				m_typeProperties.m_point.m_cubeShadowMap->ShadowCamera()->SetNearFar(glm::vec2(0.1f, deferredMeshRadius));

				m_typeProperties.m_point.m_cubeShadowMap->SetMinMaxShadowBias(glm::vec2(
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMinShadowBias),
					en::Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMaxShadowBias)));
			}
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
		: en::NamedObject("NamedObject LIGHT NAMES ARE DEPRECATED !!!!!!!!!!!!!!!")
	{
		SEAssert("This constructor is only for AmbientIBL_Deferred lights", lightType == LightType::AmbientIBL_Deferred);

		m_typeProperties.m_type = LightType::AmbientIBL_Deferred;
		m_typeProperties.m_ambient.m_IBLTex = iblTex;
	}


	void Light::Destroy()
	{
		switch (m_typeProperties.m_type)
		{
		case LightType::AmbientIBL_Deferred:
		{
			m_typeProperties.m_ambient.m_IBLTex = nullptr;
		}
		break;
		case LightType::Directional_Deferred:
		{
			m_typeProperties.m_directional.m_shadowMap = nullptr;
		}
		break;
		case LightType::Point_Deferred:
		{
			m_typeProperties.m_point.m_cubeShadowMap = nullptr;
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}
	}


	void Light::Update(const double stepTimeMs)
	{
		switch (m_typeProperties.m_type)
		{
		case LightType::AmbientIBL_Deferred:
		{
		}
		break;
		case LightType::Directional_Deferred:
		{
			if (m_typeProperties.m_directional.m_shadowMap && 
				m_typeProperties.m_directional.m_ownerTransform->HasChanged())
			{
				m_typeProperties.m_directional.m_shadowMap->UpdateShadowCameraConfig();
			}
		}
		break;
		case LightType::Point_Deferred:
		{
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}
	}


	glm::vec4 Light::GetColorIntensity() const
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
		return glm::vec4(1.f, 0.f, 1.f, 1.f); // Magenta error color
	}


	fr::ShadowMap* Light::GetShadowMap() const
	{
		switch (m_typeProperties.m_type)
		{
		case LightType::AmbientIBL_Deferred:
		{
			SEAssertF("Ambient lights do not have a shadow map");
		}
		break;
		case LightType::Directional_Deferred:
		{
			return m_typeProperties.m_directional.m_shadowMap.get();
		}
		break;
		case LightType::Point_Deferred:
		{
			return m_typeProperties.m_point.m_cubeShadowMap.get();
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}
		return nullptr;
	}


	Light::TypeProperties& Light::AccessLightTypeProperties(fr::Light::LightType lightType)
	{
		SEAssert("Trying to access type properties for the wrong type", lightType == m_typeProperties.m_type);
		return m_typeProperties;
	}


	Light::TypeProperties const& Light::AccessLightTypeProperties(LightType lightType) const
	{
		SEAssert("Trying to access type properties for the wrong type", lightType == m_typeProperties.m_type);
		return m_typeProperties;
	}


	void Light::ShowImGuiWindow()
	{
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

				// ECS_CONVERSION: TODO Restore this functionality (move it to the deferred lighting GS)
				
				//if (ImGui::CollapsingHeader(std::format("IBL Textures##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
				//{
				//	ImGui::Indent();
				//	ImGui::Text("BRDF Integration map: \"%s\"",
				//		m_typeProperties.m_ambient.m_BRDF_integrationMap->GetName().c_str());

				//	ImGui::Text("IEM Texture: \"%s\"",
				//		m_typeProperties.m_ambient.m_IEMTex->GetName().c_str());

				//	ImGui::Text("PMREM Texture: \"%s\"",
				//		m_typeProperties.m_ambient.m_PMREMTex->GetName().c_str());
				//	ImGui::Unindent();
				//}
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
	}
}

