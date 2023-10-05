// © 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "Config.h"
#include "DebugConfiguration.h"
#include "Light.h"
#include "MeshPrimitive.h"
#include "SceneManager.h"
#include "Shader.h"

using re::Shader;
using gr::Transform;
using en::Config;
using en::SceneManager;
using std::unique_ptr;
using std::make_unique;
using std::string;
using glm::vec3;
using re::ParameterBlock;


namespace
{
	gr::Camera::CameraConfig ComputeDirectionalShadowCameraConfigFromSceneBounds(
		gr::Transform* lightTransform, gr::Bounds& sceneWorldBounds)
	{
		gr::Bounds const& transformedBounds = sceneWorldBounds.GetTransformedAABBBounds(
			glm::inverse(lightTransform->GetGlobalMatrix(Transform::TRS)));

		gr::Camera::CameraConfig shadowCamConfig;

		shadowCamConfig.m_near						= -transformedBounds.zMax();
		shadowCamConfig.m_far						= -transformedBounds.zMin();
		shadowCamConfig.m_projectionType			= gr::Camera::CameraConfig::ProjectionType::Orthographic;
		shadowCamConfig.m_orthoLeftRightBotTop.x	= transformedBounds.xMin();
		shadowCamConfig.m_orthoLeftRightBotTop.y	= transformedBounds.xMax();
		shadowCamConfig.m_orthoLeftRightBotTop.z	= transformedBounds.yMin();
		shadowCamConfig.m_orthoLeftRightBotTop.w	= transformedBounds.yMax();

		return shadowCamConfig;
	}
}


namespace gr
{
	std::shared_ptr<Light> Light::CreateAmbientLight(std::string const& name)
	{
		std::shared_ptr<gr::Light> newAmbientLight;
		newAmbientLight.reset(new gr::Light(name, nullptr, Light::AmbientIBL, glm::vec3(1.f, 0.f, 1.f), false));
		en::SceneManager::GetSceneData()->AddLight(newAmbientLight);
		return newAmbientLight;
	}


	std::shared_ptr<Light> Light::CreateDirectionalLight(
		std::string const& name, gr::Transform* ownerTransform, glm::vec3 colorIntensity, bool hasShadow)
	{
		std::shared_ptr<gr::Light> newDirectionalLight;
		newDirectionalLight.reset(new gr::Light(name, ownerTransform, Light::Directional, colorIntensity, hasShadow));
		en::SceneManager::GetSceneData()->AddLight(newDirectionalLight);
		return newDirectionalLight;
	}


	std::shared_ptr<Light> Light::CreatePointLight(
		std::string const& name, gr::Transform* ownerTransform, glm::vec3 colorIntensity, bool hasShadow)
	{
		std::shared_ptr<gr::Light> newPointLight;
		newPointLight.reset(new gr::Light(name, ownerTransform, Light::Point, colorIntensity, hasShadow));
		en::SceneManager::GetSceneData()->AddLight(newPointLight);
		return newPointLight;
	}


	Light::Light(string const& name, Transform* ownerTransform, LightType lightType, vec3 colorIntensity, bool hasShadow)
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
					glm::vec3(0.f, 0.f, 0.f),
					ShadowMap::ShadowType::Single);
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

			// Compute the radius: 
			const float cutoff = 0.05f; // Want the sphere mesh radius where light intensity will be close to zero
			const float maxColor = glm::max(glm::max(colorIntensity.r, colorIntensity.g), colorIntensity.b);
			const float radius = glm::sqrt((maxColor / cutoff) - 1.0f);

			// Scale the owning transform such that a sphere created with a radius of 1 will be the correct size
			m_typeProperties.m_point.m_ownerTransform->SetLocalScale(vec3(radius, radius, radius));

			m_typeProperties.m_point.m_cubeShadowMap = nullptr;
			if (hasShadow)
			{
				gr::Camera::CameraConfig shadowCamConfig;
				shadowCamConfig.m_yFOV = static_cast<float>(std::numbers::pi) / 2.0f;
				shadowCamConfig.m_near = 0.1f;
				shadowCamConfig.m_far = radius;
				shadowCamConfig.m_aspectRatio = 1.0f;
				shadowCamConfig.m_projectionType = Camera::CameraConfig::ProjectionType::Perspective;

				const uint32_t cubeMapRes = Config::Get()->GetValue<int>("defaultShadowCubeMapRes");

				m_typeProperties.m_point.m_cubeShadowMap = make_unique<ShadowMap>(
					GetName(),
					cubeMapRes,
					cubeMapRes,
					shadowCamConfig,
					m_typeProperties.m_point.m_ownerTransform,
					vec3(0.0f, 0.0f, 0.0f),	// shadowCamPosition: No offset
					ShadowMap::ShadowType::CubeMap);

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
			// Update shadow cam bounds:
			gr::Bounds sceneWorldBounds = SceneManager::GetSceneData()->GetWorldSpaceSceneBounds();

			Camera::CameraConfig const& shadowCamConfig = ComputeDirectionalShadowCameraConfigFromSceneBounds(
				m_typeProperties.m_directional.m_ownerTransform, sceneWorldBounds);

			if (m_typeProperties.m_directional.m_shadowMap)
			{
				m_typeProperties.m_directional.m_shadowMap->ShadowCamera()->SetCameraConfig(shadowCamConfig);
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


	glm::vec3 Light::GetColor() const
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
		return glm::vec3(1.f, 0.f, 1.f); // Magenta error color
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

		auto ShowColorPicker = [&uniqueID](std::string const& lightName, glm::vec3& color)
		{
			ImGui::Text("Color:"); ImGui::SameLine();
			ImGuiColorEditFlags flags = ImGuiColorEditFlags_HDR;
			ImGui::ColorEdit4(
				std::format("{} {}", lightName, "Color").c_str(), 
				&color.r, 
				ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | flags);
		};

		auto ShowDebugOptions = [this, &uniqueID]()
		{
			const bool currentIsEnabled = m_typeProperties.m_diffuseEnabled || m_typeProperties.m_specularEnabled;

			bool newEnabled = currentIsEnabled;
			ImGui::Checkbox(std::format("Enabled?##{}", uniqueID).c_str(), &newEnabled);
			if (newEnabled != currentIsEnabled)
			{
				m_typeProperties.m_diffuseEnabled = newEnabled;
				m_typeProperties.m_specularEnabled = newEnabled;
			}

			if (ImGui::CollapsingHeader(std::format("Debug##{}{}", GetName(), uniqueID).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Checkbox("Diffuse enabled", &m_typeProperties.m_diffuseEnabled);
				ImGui::Checkbox("Specular enabled", &m_typeProperties.m_specularEnabled);
			}
		};
		

		if (ImGui::CollapsingHeader(std::format("{}##{}", GetName(), uniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			switch (m_type)
			{
			case LightType::AmbientIBL:
			{				
				ImGui::SliderFloat(
					"Ambient scale", 
					&m_typeProperties.m_intensityScale,
					0.0f, 
					4.0f, 
					"%.3f", 
					ImGuiSliderFlags_None);

				ImGui::Text("BRDF Integration map: \"%s\"", 
					m_typeProperties.m_ambient.m_BRDF_integrationMap->GetName().c_str());

				ImGui::Text("IEM Texture: \"%s\"",
					m_typeProperties.m_ambient.m_IEMTex->GetName().c_str());

				ImGui::Text("PMREM Texture: \"%s\"",
					m_typeProperties.m_ambient.m_PMREMTex->GetName().c_str());
			}
			break;
			case LightType::Directional:
			{
				m_typeProperties.m_directional.m_ownerTransform->ShowImGuiWindow();

				ShowColorPicker(GetName(), m_typeProperties.m_directional.m_colorIntensity);

				if (m_typeProperties.m_directional.m_shadowMap)
				{
					ImGui::Text("Shadow map: \"%s\"", GetName().c_str());
					m_typeProperties.m_directional.m_shadowMap->ShowImGuiWindow();
				}
				else
				{
					ImGui::Text("<No Shadow>");
				}
			}
			break;
			case LightType::Point:
			{
				m_typeProperties.m_point.m_ownerTransform->ShowImGuiWindow();

				ShowColorPicker(GetName(), m_typeProperties.m_point.m_colorIntensity);

				if (m_typeProperties.m_point.m_cubeShadowMap)
				{
					ImGui::Text("Cube Shadow map: \"%s\"", GetName().c_str());
					m_typeProperties.m_point.m_cubeShadowMap->ShowImGuiWindow();
				}
				else
				{
					ImGui::Text("<No Shadow>");
				}
			}
			break;
			default:
				SEAssertF("Invalid light type");
			}

			ShowDebugOptions();
		}	
	}
}

