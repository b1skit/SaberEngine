#include <memory>

#include "ShadowMap.h"
#include "CoreEngine.h"
#include "Camera.h"
#include "Scene.h"
#include "Texture.h"
#include "Material.h"
using gr::Material;
using gr::Texture;


namespace SaberEngine
{
	ShadowMap::ShadowMap(
		string lightName, 
		uint32_t xRes,
		uint32_t yRes,
		CameraConfig shadowCamConfig, 
		Transform* shadowCamParent /*= nullptr*/, 
		vec3 shadowCamPosition /* = vec3(0.0f, 0.0f, 0.0f)*/, 
		bool useCubeMap /*= false*/)
	{
		m_shadowCam = std::make_shared<Camera>(lightName + "_ShadowMapCam", shadowCamConfig, shadowCamParent);
		m_shadowCam->GetTransform()->SetWorldPosition(shadowCamPosition);

		// Texture params are mostly the same between a single shadow map, or a cube map
		Texture::TextureParams shadowParams;
		shadowParams.m_width = xRes;
		shadowParams.m_height = yRes;
		shadowParams.m_texUse = Texture::TextureUse::DepthTarget;
		shadowParams.m_texFormat = Texture::TextureFormat::Depth32F;
		shadowParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
		shadowParams.m_texSamplerMode = Texture::TextureSamplerMode::Clamp;
		shadowParams.m_texMinMode = Texture::TextureMinFilter::Linear;
		shadowParams.m_texMaxMode = Texture::TextureMaxFilter::Linear;
		shadowParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		shadowParams.m_useMIPs = false;

		// TODO: Enable Anisotropic filtering for shadows
		// http://www.joshbarczak.com/blog/?p=396
		// https://www.khronos.org/opengl/wiki/Sampler_Object#Anisotropic_filtering

		std::shared_ptr<gr::Texture> depthTexture;
		std::string shaderName;
		uint32_t textureUnit = 0;

		// Omni-directional (Cube map) shadowmap setup:
		if (useCubeMap)
		{
			shadowParams.m_texDimension = Texture::TextureDimension::TextureCubeMap;
			shadowParams.m_texturePath = lightName + "_CubeShadowMap";
			shadowParams.m_faces = Texture::k_numCubeFaces;

			depthTexture = std::make_shared<gr::Texture>(shadowParams);

			textureUnit = Material::CubeMap0;
			shaderName = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("cubeDepthShaderName");
		}
		else // Single texture shadowmap setup:
		{
			shadowParams.m_texDimension = Texture::TextureDimension::Texture2D;
			shadowParams.m_texturePath = lightName + "_SingleShadowMap";
			shadowParams.m_faces = 1;
			
			depthTexture = std::make_shared<gr::Texture>(shadowParams);

			textureUnit = Material::Depth0;
			shaderName = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("depthShaderName");
		}

		m_shadowCam->GetRenderShader() = Shader::CreateShader(shaderName);

		m_shadowTargetSet.DepthStencilTarget() = depthTexture;
		m_shadowTargetSet.Viewport() = gr::Viewport(0, 0, depthTexture->Width(), depthTexture->Height());
		m_shadowTargetSet.CreateDepthStencilTarget(textureUnit);

		CoreEngine::GetSceneManager()->RegisterCamera(CAMERA_TYPE_SHADOW, m_shadowCam);
	}
}


