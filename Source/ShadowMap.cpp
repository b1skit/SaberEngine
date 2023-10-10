// © 2022 Adam Badke. All rights reserved.
#include "Camera.h"
#include "Config.h"
#include "Light.h"
#include "Material.h"
#include "SceneData.h"
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
		case gr::Light::LightType::Directional: return gr::ShadowMap::ShadowType::Single;
		case gr::Light::LightType::Point: return gr::ShadowMap::ShadowType::CubeMap;
		case gr::Light::LightType::AmbientIBL:
		default:
			SEAssertF("Invalid or unsupported light type for shadow map");
		}
		return gr::ShadowMap::ShadowType::ShadowType_Count;
	}	
}

namespace gr
{
	ShadowMap::ShadowMap(
		string const& lightName,
		uint32_t xRes,
		uint32_t yRes,
		gr::Camera::CameraConfig shadowCamConfig, 
		Transform* shadowCamParent, 
		vec3 shadowCamPosition, 
		gr::Light* owningLight)
		: NamedObject(lightName + "_Shadow")
		, m_owningLight(owningLight)
		, m_shadowCam(gr::Camera::Create(lightName + "_ShadowCam", shadowCamConfig, shadowCamParent))
	{
		SEAssert("Owning light cannot be null", owningLight);

		m_shadowTargetSet = re::TextureTargetSet::Create(lightName + "_ShadowTargetSet");
		m_shadowCam->GetTransform()->SetLocalTranslation(shadowCamPosition);

		// Texture params are mostly the same between a single shadow map, or a cube map
		Texture::TextureParams shadowParams;
		shadowParams.m_width = xRes;
		shadowParams.m_height = yRes;
		shadowParams.m_usage = Texture::Usage::DepthTarget;
		shadowParams.m_format = Texture::Format::Depth32F;
		shadowParams.m_colorSpace = Texture::ColorSpace::Linear;
		shadowParams.m_mipMode = re::Texture::MipMode::None;
		shadowParams.m_addToSceneData = false;
		shadowParams.m_clear.m_depthStencil.m_depth = 1.f;

		// TODO: Enable mipmaps + anisotropic filtering for shadows
		// http://www.joshbarczak.com/blog/?p=396
		// https://www.khronos.org/opengl/wiki/Sampler_Object#Anisotropic_filtering

		std::shared_ptr<re::Texture> depthTexture;

		const ShadowType shadowType = GetShadowTypeFromLightType(owningLight->Type());

		re::TextureTarget::TargetParams depthTargetParams;

		// Omni-directional (Cube map) shadow map setup:
		if (shadowType == ShadowType::CubeMap)
		{
			m_minMaxShadowBias = glm::vec2(
				Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultPointLightMinShadowBias),
				Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias));

			shadowParams.m_dimension = Texture::Dimension::TextureCubeMap;
			shadowParams.m_faces = 6;
			const string texName = lightName + "_CubeMapShadow";

			depthTexture = re::Texture::Create(texName, shadowParams, false);

			depthTargetParams.m_targetFace = re::TextureTarget::k_allFaces;
		}
		else // 2D shadow map setup:
		{
			m_minMaxShadowBias = glm::vec2(
				Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMinShadowBias),
				Config::Get()->GetValue<float>(en::ConfigKeys::k_defaultDirectionalLightMaxShadowBias));

			shadowParams.m_dimension = Texture::Dimension::Texture2D;
			shadowParams.m_faces = 1;
			const string texName = lightName + "_Shadow";
			
			depthTexture = re::Texture::Create(texName, shadowParams, false);
		}

		m_shadowTargetSet->SetDepthStencilTarget(depthTexture, depthTargetParams);
		m_shadowTargetSet->SetViewport(re::Viewport(0, 0, depthTexture->Width(), depthTexture->Height()));
		m_shadowTargetSet->SetScissorRect(
			{0, 0, static_cast<long>(depthTexture->Width()), static_cast<long>(depthTexture->Height()) });
	}


	void ShadowMap::SetMinMaxShadowBias(glm::vec2 const& minMaxShadowBias)
	{
		m_minMaxShadowBias = minMaxShadowBias;
	}


	void ShadowMap::ShowImGuiWindow()
	{
		ImGui::Text("Name: \"%s\"", GetName().c_str());

		const std::string minLabel = "Min shadow bias##" + GetName();
		ImGui::SliderFloat(minLabel.c_str(), &m_minMaxShadowBias.x, 0, 0.1f, "Min shadow bias = %.5f");
		
		const std::string maxLabel = "Max shadow bias##" + GetName();
		ImGui::SliderFloat(maxLabel.c_str(), &m_minMaxShadowBias.y, 0, 0.1f, "Max shadow bias = %.5f");

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
			default: SEAssertF("Invalid/unsupported light type")
			}
		}

		if (ImGui::TreeNode("Shadow Map Camera:"))
		{
			m_shadowCam->ShowImGuiWindow();

			ImGui::TreePop();
		}		
	}
}


