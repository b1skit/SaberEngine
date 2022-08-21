#include "Skybox.h"
#include "Material.h"
#include "Mesh.h"
#include "CoreEngine.h"
#include "BuildConfiguration.h"
#include "ImageBasedLight.h"
#include "Shader.h"
#include "Texture.h"


namespace SaberEngine
{
	Skybox::Skybox(std::shared_ptr<Material> skyMaterial, std::shared_ptr<gr::Mesh> skyMesh)
	{
		m_skyMaterial	= skyMaterial;
		m_skyMesh		= skyMesh;
	}


	Skybox::Skybox(string sceneName)
	{
		// Create a cube map material
		m_skyMaterial = std::make_shared<Material>("SkyboxMaterial", std::shared_ptr<Shader>(nullptr), CUBE_MAP_NUM_FACES, false);

		// Attempt to load a HDR image:
		std::shared_ptr<gr::Texture> iblAsSkyboxCubemap = 
			(std::shared_ptr<gr::Texture>)ImageBasedLight::ConvertEquirectangularToCubemap(
				CoreEngine::GetSceneManager()->GetCurrentSceneName(), 
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("defaultIBLPath"), 
				1024, 
				1024); // TODO: Parameterize cubemap dimensions?

		// NOTE: ConvertEquirectangularToCubemap() buffers the texture

		if (iblAsSkyboxCubemap != nullptr)
		{
			LOG("Successfully loaded IBL HDR texture for skybox");

			m_skyMaterial->AttachCubeMapTextures(iblAsSkyboxCubemap);
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

			m_skyMaterial->AccessTexture((TEXTURE_TYPE)0) = cubemapTexture;
		}


		// Create a skybox shader, now that we have some sort of image loaded:
		std::shared_ptr<Shader> skyboxShader = Shader::CreateShader(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("skyboxShaderName"));

		m_skyMaterial->GetShader() = skyboxShader;

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
		if (m_skyMaterial != nullptr)
		{
			m_skyMaterial = nullptr;
		}

		if (m_skyMesh != nullptr)
		{
			m_skyMesh = nullptr;
		}
	}
}


