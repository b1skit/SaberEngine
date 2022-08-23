#include "Skybox.h"
#include "Mesh.h"
#include "CoreEngine.h"
#include "BuildConfiguration.h"
#include "ImageBasedLight.h"
#include "Shader.h"
#include "Texture.h"
using gr::Texture;
using gr::Shader;
using std::shared_ptr;
using std::make_shared;


namespace SaberEngine
{
	Skybox::Skybox(std::string const& sceneName)
	{
		// Attempt to load a HDR image:
		m_skyTexture = ImageBasedLight::ConvertEquirectangularToCubemap(
				CoreEngine::GetSceneManager()->GetCurrentSceneName(), 
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("defaultIBLPath"), 
				1024, 
				1024); // TODO: Parameterize cubemap dimensions?

		// NOTE: ConvertEquirectangularToCubemap() buffers the texture

		if (m_skyTexture != nullptr)
		{
			LOG("Successfully loaded IBL HDR texture for skybox");
		}
		else // Attempt to create Skybox from 6x skybox textures:
		{
			string skyboxTextureRoot =
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("sceneRoot") + sceneName + "\\Skybox\\";

			// Track the textures as we load them:
			std::shared_ptr<gr::Texture> cubemapTexture(nullptr);

			cubemapTexture = 
				gr::Texture::LoadCubeMapTextureFilesFromPath(skyboxTextureRoot, gr::Texture::TextureColorSpace::sRGB);

			gr::Texture::TextureParams cubemapParams = cubemapTexture->GetTextureParams();
			cubemapParams.m_texSamplerMode = gr::Texture::TextureSamplerMode::Clamp;
			cubemapParams.m_texMinMode = gr::Texture::TextureMinFilter::Linear;
			cubemapParams.m_texMaxMode = gr::Texture::TextureMaxFilter::Linear;
			cubemapParams.m_texFormat = gr::Texture::TextureFormat::RGBA8;
			cubemapTexture->SetTextureParams(cubemapParams);

			m_skyTexture = cubemapTexture;
		}


		// Create a skybox shader, now that we have some sort of image loaded:
		m_skyShader = make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("skyboxShaderName"));
		m_skyShader->Create();

		// Create a quad at furthest point in the depth buffer
		m_skyMesh =	gr::meshfactory::CreateQuad
		(
			vec3(-1.0f, 1.0f,	1.0f), // z == 1.0f, since we're in clip space (and camera's negative Z has been reversed)
			vec3(1.0f,	1.0f,	1.0f),
			vec3(-1.0f, -1.0f,	1.0f),
			vec3(1.0f,	-1.0f,	1.0f)
		);

		m_skyMesh->Name() = "SkyboxQuad";
	}


	Skybox::~Skybox()
	{
		m_skyTexture = nullptr;
		m_skyShader = nullptr;
		m_skyMesh = nullptr;
	}
}


