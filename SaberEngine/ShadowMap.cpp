#include "Camera.h"
#include "Config.h"
#include "Material.h"
#include "SceneData.h"
#include "ShadowMap.h"
#include "Texture.h"


namespace gr
{
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


	ShadowMap::ShadowMap(
		string lightName, 
		uint32_t xRes,
		uint32_t yRes,
		gr::Camera::CameraConfig shadowCamConfig, 
		Transform* shadowCamParent, 
		vec3 shadowCamPosition, 
		bool useCubeMap)
		: m_shadowCam(lightName + "_ShadowMapCam", shadowCamConfig, shadowCamParent)
		, m_minMaxShadowBias(0.005f, 0.0005f)
	{
		m_shadowTargetSet = std::make_shared<re::TextureTargetSet>(lightName + " target");
		m_shadowCam.GetTransform()->SetLocalTranslation(shadowCamPosition);

		// Texture params are mostly the same between a single shadow map, or a cube map
		Texture::TextureParams shadowParams;
		shadowParams.m_width = xRes;
		shadowParams.m_height = yRes;
		shadowParams.m_usage = Texture::Usage::DepthTarget;
		shadowParams.m_format = Texture::Format::Depth32F;
		shadowParams.m_colorSpace = Texture::ColorSpace::Linear;
		shadowParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		shadowParams.m_useMIPs = false;

		// TODO: Enable mipmaps + anisotropic filtering for shadows
		// http://www.joshbarczak.com/blog/?p=396
		// https://www.khronos.org/opengl/wiki/Sampler_Object#Anisotropic_filtering

		std::shared_ptr<re::Texture> depthTexture;

		// Omni-directional (Cube map) shadowmap setup:
		if (useCubeMap)
		{
			shadowParams.m_dimension = Texture::Dimension::TextureCubeMap;
			shadowParams.m_faces = 6;
			const string texName = lightName + "_CubeShadowMap";

			depthTexture = std::make_shared<re::Texture>(texName, shadowParams);
		}
		else // Single texture shadowmap setup:
		{
			shadowParams.m_dimension = Texture::Dimension::Texture2D;
			shadowParams.m_faces = 1;
			const string texName = lightName + "_SingleShadowMap";
			
			depthTexture = std::make_shared<re::Texture>(texName, shadowParams);
		}

		m_shadowTargetSet->SetDepthStencilTarget(depthTexture);
		m_shadowTargetSet->Viewport() = re::Viewport(0, 0, depthTexture->Width(), depthTexture->Height());
	}
}


