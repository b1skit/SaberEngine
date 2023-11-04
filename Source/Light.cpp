// © 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "Config.h"
#include "DebugConfiguration.h"
#include "Light.h"
#include "MeshPrimitive.h"
#include "SceneManager.h"
#include "Shader.h"
#include "ShadowMap.h"

using re::Shader;
using gr::Transform;
using en::Config;
using en::SceneManager;
using std::unique_ptr;
using std::make_unique;
using std::string;
using re::ParameterBlock;


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


	void ConfigurePointLightMeshScale(gr::Light* pointLight)
	{
		SEAssert("Light is not a point light", pointLight->Type() == gr::Light::LightType::Point);

		gr::Light::LightTypeProperties& lightProperties = 
			pointLight->AccessLightTypeProperties(gr::Light::LightType::Point);

		const float newDeferredMeshRadius = ComputePointLightMeshRadiusScaleFromIntensity(
			lightProperties.m_point.m_colorIntensity.a,
			lightProperties.m_point.m_emitterRadius,
			lightProperties.m_point.m_intensityCuttoff);

		// Scale the owning transform such that a sphere created with a radius of 1 will be the correct size
		lightProperties.m_point.m_ownerTransform->SetLocalScale(
			glm::vec3(newDeferredMeshRadius, newDeferredMeshRadius, newDeferredMeshRadius));
	}
}


namespace gr
{
	std::shared_ptr<Light> Light::CreateAmbientLight(std::string const& name)
	{
		std::shared_ptr<gr::Light> newAmbientLight;
		newAmbientLight.reset(new gr::Light(name, nullptr, Light::AmbientIBL, glm::vec4(1.f, 0.f, 1.f, 1.f), false));
		en::SceneManager::GetSceneData()->AddLight(newAmbientLight);
		return newAmbientLight;
	}


	std::shared_ptr<Light> Light::CreateDirectionalLight(
		std::string const& name, gr::Transform* ownerTransform, glm::vec4 colorIntensity, bool hasShadow)
	{
		std::shared_ptr<gr::Light> newDirectionalLight;
		newDirectionalLight.reset(new gr::Light(name, ownerTransform, Light::Directional, colorIntensity, hasShadow));
		en::SceneManager::GetSceneData()->AddLight(newDirectionalLight);
		return newDirectionalLight;
	}


	std::shared_ptr<Light> Light::CreatePointLight(
		std::string const& name, gr::Transform* ownerTransform, glm::vec4 colorIntensity, bool hasShadow)
	{
		std::shared_ptr<gr::Light> newPointLight;
		newPointLight.reset(new gr::Light(name, ownerTransform, Light::Point, colorIntensity, hasShadow));
		en::SceneManager::GetSceneData()->AddLight(newPointLight);
		return newPointLight;
	}


	Light::Light(string const& name, Transform* ownerTransform, LightType lightType, glm::vec4 colorIntensity, bool hasShadow)
		: en::NamedObject(name)
		, m_type(lightType)
	{
		switch (lightType)
		{
		case AmbientIBL:
		{
		}
		break;
		case Directional:
		{
			m_typeProperties.m_directional.m_ownerTransform = ownerTransform;
			m_typeProperties.m_directional.m_colorIntensity = colorIntensity;

			m_typeProperties.m_directional.m_shadowMap = nullptr;
			if (hasShadow)
			{
				const uint32_t shadowMapRes = Config::Get()->GetValue<int>("defaultShadowMapRes");
				m_typeProperties.m_directional.m_shadowMap = make_unique<ShadowMap>(
					GetName(),
					shadowMapRes,
					shadowMapRes,
					Camera::CameraConfig(),
					m_typeProperties.m_directional.m_ownerTransform,
					glm::vec3(0.f, 0.f, 0.f), // Shadow cam position
					this);
				// Note: We'll compute the camera config from the scene bounds during the first call to Update(); so
				// here we just pass a default camera config

				m_typeProperties.m_directional.m_shadowMap->SetMinMaxShadowBias(glm::vec2(
					Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMinShadowBias),
					Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias)));
			}
		}
		break;
		case Point:
		{
			m_typeProperties.m_point.m_ownerTransform = ownerTransform;
			m_typeProperties.m_point.m_colorIntensity = colorIntensity;

			m_typeProperties.m_point.m_emitterRadius = 0.1f;
			m_typeProperties.m_point.m_intensityCuttoff = 0.05f;

			ConfigurePointLightMeshScale(this);
			
			const float deferredMeshRadius = m_typeProperties.m_point.m_ownerTransform->GetLocalScale().x;

			m_typeProperties.m_point.m_cubeShadowMap = nullptr;
			if (hasShadow)
			{
				gr::Camera::CameraConfig shadowCamConfig;
				shadowCamConfig.m_yFOV = static_cast<float>(std::numbers::pi) / 2.0f;
				shadowCamConfig.m_near = 0.1f;
				shadowCamConfig.m_far = deferredMeshRadius;
				shadowCamConfig.m_aspectRatio = 1.0f;
				shadowCamConfig.m_projectionType = Camera::CameraConfig::ProjectionType::Perspective;

				const uint32_t cubeMapRes = Config::Get()->GetValue<int>("defaultShadowCubeMapRes");

				m_typeProperties.m_point.m_cubeShadowMap = make_unique<ShadowMap>(
					GetName(),
					cubeMapRes,
					cubeMapRes,
					shadowCamConfig,
					m_typeProperties.m_point.m_ownerTransform,
					glm::vec3(0.0f, 0.0f, 0.0f),	// Shadow cam position: No offset
					this);

				m_typeProperties.m_point.m_cubeShadowMap->SetMinMaxShadowBias(glm::vec2(
					Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMinShadowBias),
					Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMaxShadowBias)));
			}
		}
		break;
		//case Spot:
		//case Area:
		//case Tube:
		default:
			// TODO: Implement light meshes for additional light types
			break;
		}
	}


	void Light::Destroy()
	{
		switch (m_type)
		{
		case LightType::AmbientIBL:
		{
			m_typeProperties.m_ambient.m_BRDF_integrationMap = nullptr;
			m_typeProperties.m_ambient.m_IEMTex = nullptr;
			m_typeProperties.m_ambient.m_PMREMTex = nullptr;
		}
		break;
		case LightType::Directional:
		{
			m_typeProperties.m_directional.m_shadowMap = nullptr;
		}
		break;
		case LightType::Point:
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
		switch (m_type)
		{
		case LightType::AmbientIBL:
		{
		}
		break;
		case LightType::Directional:
		{
			if (m_typeProperties.m_directional.m_shadowMap)
			{
				// TODO: We should only do this if something has actually changed
				m_typeProperties.m_directional.m_shadowMap->UpdateShadowCameraConfig();
			}
		}
		break;
		case LightType::Point:
		{
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}
	}


	glm::vec4 Light::GetColorIntensity() const
	{
		switch (m_type)
		{
		case LightType::AmbientIBL:
		{
			SEAssertF("Ambient lights don't (current) have a color/intensity value");
		}
		break;
		case LightType::Directional:
		{
			return m_typeProperties.m_directional.m_colorIntensity;
		}
		break;
		case LightType::Point:
		{
			return m_typeProperties.m_point.m_colorIntensity;
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}
		return glm::vec4(1.f, 0.f, 1.f, 1.f); // Magenta error color
	}


	gr::ShadowMap* Light::GetShadowMap() const
	{
		switch (m_type)
		{
		case LightType::AmbientIBL:
		{
			SEAssertF("Ambient lights do not have a shadow map");
		}
		break;
		case LightType::Directional:
		{
			return m_typeProperties.m_directional.m_shadowMap.get();
		}
		break;
		case LightType::Point:
		{
			return m_typeProperties.m_point.m_cubeShadowMap.get();
		}
		break;
		default:
			SEAssertF("Invalid light type");
		}
		return nullptr;
	}


	Light::LightTypeProperties& Light::AccessLightTypeProperties(Light::LightType lightType)
	{
		SEAssert("Trying to access type properties for the wrong type", lightType == m_type);
		return m_typeProperties;
	}


	Light::LightTypeProperties const& Light::AccessLightTypeProperties(LightType lightType) const
	{
		SEAssert("Trying to access type properties for the wrong type", lightType == m_type);
		return m_typeProperties;
	}


	void Light::ShowImGuiWindow()
	{
		const uint64_t uniqueID = GetUniqueID();

		auto ShowDebugOptions = [this, &uniqueID]()
		{
			if (ImGui::CollapsingHeader(std::format("Debug##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Checkbox(std::format("Diffuse enabled##{}", uniqueID).c_str(), &m_typeProperties.m_diffuseEnabled);
				ImGui::Checkbox(std::format("Specular enabled##{}", uniqueID).c_str(), &m_typeProperties.m_specularEnabled);
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

		auto ShowShadowMapMenu = [this, &uniqueID](gr::ShadowMap* shadowMap)
		{
			if (ImGui::CollapsingHeader(std::format("Shadow map##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
			{
				if (shadowMap)
				{
					shadowMap->ShowImGuiWindow();
				}
				else
				{
					ImGui::Text("<No Shadow>");
				}
			}
		};

		auto ShowTransformMenu = [this, &uniqueID](gr::Transform* transform)
		{
			if (ImGui::CollapsingHeader(std::format("Transform##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
			{
				transform->ShowImGuiWindow();
			}
		};

		if (ImGui::CollapsingHeader(std::format("{}##{}", GetName(), uniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			switch (m_type)
			{
			case LightType::AmbientIBL:
			{
				ShowCommonOptions(nullptr);

				if (ImGui::CollapsingHeader(std::format("IBL Textures##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
				{
					ImGui::Text("BRDF Integration map: \"%s\"",
						m_typeProperties.m_ambient.m_BRDF_integrationMap->GetName().c_str());

					ImGui::Text("IEM Texture: \"%s\"",
						m_typeProperties.m_ambient.m_IEMTex->GetName().c_str());

					ImGui::Text("PMREM Texture: \"%s\"",
						m_typeProperties.m_ambient.m_PMREMTex->GetName().c_str());
				}
			}
			break;
			case LightType::Directional:
			{
				ShowCommonOptions(&m_typeProperties.m_directional.m_colorIntensity);

				ShowShadowMapMenu(m_typeProperties.m_directional.m_shadowMap.get());
				ShowTransformMenu(m_typeProperties.m_directional.m_ownerTransform);
			}
			break;
			case LightType::Point:
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
		}	
	}
}

