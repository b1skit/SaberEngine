// © 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "Config.h"
#include "GameplayManager.h"
#include "Light.h"
#include "Material.h"
#include "SceneData.h"
#include "SceneManager.h"
#include "ShadowMap.h"
#include "Texture.h"

using en::Config;
using gr::Material;
using re::Texture;
using re::Shader;
using gr::Camera;
using gr::Transform;
using std::shared_ptr;
using std::make_shared;
using std::string;
using glm::vec3;


namespace
{
	gr::ShadowMap::ShadowType GetShadowTypeFromLightType(gr::Light::LightType lightType)
	{
		switch (lightType)
		{
		case gr::Light::LightType::Directional: return gr::ShadowMap::ShadowType::Orthographic;
		case gr::Light::LightType::Point: return gr::ShadowMap::ShadowType::CubeMap;
		case gr::Light::LightType::AmbientIBL:
		default:
			SEAssertF("Invalid or unsupported light type for shadow map");
		}
		return gr::ShadowMap::ShadowType::Invalid;
	}


	gr::Camera::Config ComputeDirectionalShadowCameraConfigFromSceneBounds(
		gr::Transform* lightTransform, fr::Bounds& sceneWorldBounds)
	{
		fr::Bounds const& transformedBounds = sceneWorldBounds.GetTransformedAABBBounds(
			glm::inverse(lightTransform->GetGlobalMatrix()));

		gr::Camera::Config shadowCamConfig;

		shadowCamConfig.m_projectionType = gr::Camera::Config::ProjectionType::Orthographic;

		shadowCamConfig.m_yFOV = 0.f; // Orthographic

		shadowCamConfig.m_near = -transformedBounds.zMax();
		shadowCamConfig.m_far = -transformedBounds.zMin();

		shadowCamConfig.m_orthoLeftRightBotTop.x = transformedBounds.xMin();
		shadowCamConfig.m_orthoLeftRightBotTop.y = transformedBounds.xMax();
		shadowCamConfig.m_orthoLeftRightBotTop.z = transformedBounds.yMin();
		shadowCamConfig.m_orthoLeftRightBotTop.w = transformedBounds.yMax();

		return shadowCamConfig;
	}
}

namespace gr
{
	ShadowMap::ShadowMap(
		string const& lightName,
		uint32_t xRes,
		uint32_t yRes,
		Transform* shadowCamParent, 
		vec3 shadowCamPosition, 
		gr::Light* owningLight)
		: NamedObject(lightName + "_Shadow")
		, m_shadowType(GetShadowTypeFromLightType(owningLight->Type()))
		, m_owningLight(owningLight)
		, m_shadowCam(nullptr)
	{
		SEAssert("Owning light cannot be null", owningLight);
		
		// Texture params are mostly the same between a single shadow map, or a cube map
		Texture::TextureParams shadowParams;
		shadowParams.m_width = xRes;
		shadowParams.m_height = yRes;
		shadowParams.m_usage = static_cast<Texture::Usage>(Texture::Usage::DepthTarget | Texture::Usage::Color);
		shadowParams.m_format = Texture::Format::Depth32F;
		shadowParams.m_colorSpace = Texture::ColorSpace::Linear;
		shadowParams.m_mipMode = re::Texture::MipMode::None;
		shadowParams.m_addToSceneData = false;
		shadowParams.m_clear.m_depthStencil.m_depth = 1.f;

		// TODO: Enable mipmaps + anisotropic filtering for shadows
		// http://www.joshbarczak.com/blog/?p=396
		// https://www.khronos.org/opengl/wiki/Sampler_Object#Anisotropic_filtering

		std::shared_ptr<re::Texture> depthTexture;
		re::TextureTarget::TargetParams depthTargetParams;
		gr::Camera::Config shadowCamConfig{};

		// Omni-directional (Cube map) shadow map setup:
		switch (m_shadowType)
		{
		case ShadowType::CubeMap:
		{
			m_minMaxShadowBias = glm::vec2(
				Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMinShadowBias),
				Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias));

			shadowParams.m_dimension = Texture::Dimension::TextureCubeMap;
			shadowParams.m_faces = 6;
			const string texName = lightName + "_CubeMapShadow";

			depthTexture = re::Texture::Create(texName, shadowParams, false);

			depthTargetParams.m_targetFace = re::TextureTarget::k_allFaces;

			shadowCamConfig.m_yFOV = static_cast<float>(std::numbers::pi) / 2.0f;
			shadowCamConfig.m_near = 0.1f;
			shadowCamConfig.m_far = 50.f;
			shadowCamConfig.m_aspectRatio = 1.0f;
			shadowCamConfig.m_projectionType = Camera::Config::ProjectionType::PerspectiveCubemap;
		}
		break;
		case ShadowType::Orthographic:
		{
			m_minMaxShadowBias = glm::vec2(
				Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMinShadowBias),
				Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias));

			shadowParams.m_dimension = Texture::Dimension::Texture2D;
			shadowParams.m_faces = 1;
			const string texName = lightName + "_Shadow";

			depthTexture = re::Texture::Create(texName, shadowParams, false);
		}
		break;
		default: SEAssertF("Invalid ShadowType");
		}

		m_shadowTargetSet = re::TextureTargetSet::Create(lightName + "_ShadowTargetSet");
		m_shadowTargetSet->SetDepthStencilTarget(depthTexture, depthTargetParams);
		m_shadowTargetSet->SetViewport(re::Viewport(0, 0, depthTexture->Width(), depthTexture->Height()));
		m_shadowTargetSet->SetScissorRect(
			{0, 0, static_cast<long>(depthTexture->Width()), static_cast<long>(depthTexture->Height()) });


		// Shadow camera:
		m_shadowCam = gr::Camera::Create(lightName + "_ShadowCam", shadowCamConfig, shadowCamParent);

		m_shadowCam->GetTransform()->SetLocalPosition(shadowCamPosition);

		// We need the scene bounds to be finalized before we can compute camera frustums; Register for a callback to
		// ensure the scene is loaded before we try
		en::SceneManager::GetSceneData()->RegisterForPostLoadCallback([&]() { UpdateShadowCameraConfig(); });
	}


	void ShadowMap::UpdateShadowCameraConfig()
	{
		switch (m_shadowType)
		{
		case gr::ShadowMap::ShadowType::Orthographic:
		{
			fr::GameplayManager& gpm = *fr::GameplayManager::Get();
			
			// Update shadow cam bounds:
			fr::Bounds sceneWorldBounds = gpm.GetSceneBounds();

			Camera::Config const& shadowCamConfig = ComputeDirectionalShadowCameraConfigFromSceneBounds(
				m_owningLight->GetTransform(), sceneWorldBounds);

			m_shadowCam->SetCameraConfig(shadowCamConfig);

		}
		break;
		case gr::ShadowMap::ShadowType::CubeMap:
		{
			// TODO...
		}
		break;
		default: SEAssertF("Invalid shadow type");
		}
	}


	void ShadowMap::SetMinMaxShadowBias(glm::vec2 const& minMaxShadowBias)
	{
		m_minMaxShadowBias = minMaxShadowBias;
	}


	void ShadowMap::ShowImGuiWindow()
	{
		ImGui::Text("Name: \"%s\"", GetName().c_str());

		const std::string minLabel = "Min shadow bias##" + GetName();
		ImGui::SliderFloat(minLabel.c_str(), &m_minMaxShadowBias.x, 0, 0.1f, "%.5f");
		
		const std::string maxLabel = "Max shadow bias##" + GetName();
		ImGui::SliderFloat(maxLabel.c_str(), &m_minMaxShadowBias.y, 0, 0.1f, "%.5f");

		const std::string resetLabel = "Reset biases to defaults##" + GetName();
		if (ImGui::Button(resetLabel.c_str()))
		{
			switch (m_owningLight->Type())
			{
			case gr::Light::LightType::Directional:
			{
				m_minMaxShadowBias = glm::vec2(
					Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMinShadowBias),
					Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias));
			}
			break;
			case gr::Light::LightType::Point:
			{
				m_minMaxShadowBias = glm::vec2(
					Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMinShadowBias),
					Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMaxShadowBias));
			}
			break;
			default: SEAssertF("Invalid/unsupported light type");
			}
		}

		if (ImGui::CollapsingHeader("Shadow Map Camera", ImGuiTreeNodeFlags_None))
		{
			m_shadowCam->ShowImGuiWindow();
		}		
	}
}


