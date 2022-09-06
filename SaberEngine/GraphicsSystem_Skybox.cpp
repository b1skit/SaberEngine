#include <memory>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "CoreEngine.h"
#include "ImageBasedLight.h"
#include "Texture.h"

using gr::Texture;
using gr::DeferredLightingGraphicsSystem;
using gr::GBufferGraphicsSystem;
using SaberEngine::ImageBasedLight;
using SaberEngine::CoreEngine;
using std::shared_ptr;
using std::string;
using glm::vec3;
using glm::mat4;


namespace gr
{
	SkyboxGraphicsSystem::SkyboxGraphicsSystem(std::string name) : GraphicsSystem(name),
		m_skyboxStage("Skybox stage"),
		m_skyTexture(nullptr)
	{
	}


	void SkyboxGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		// Attempt to load a HDR skybox image:
		m_skyTexture = ImageBasedLight::ConvertEquirectangularToCubemap(
			CoreEngine::GetSceneManager()->GetCurrentSceneName(),
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("defaultIBLPath"),
			1024,
			1024); // TODO: Parameterize cubemap dimensions?

		if (m_skyTexture == nullptr) // Attempt to create Skybox from 6x skybox cubemap faces:
		{
			const string& sceneName = CoreEngine::GetSceneManager()->GetCurrentSceneName();
			const string skyboxTextureRoot =
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("sceneRoot") + sceneName + "\\Skybox\\";
			// TODO: This skybox path should be user-configurable

			shared_ptr<Texture> m_skyTexture =
				Texture::LoadCubeMapTextureFilesFromPath(skyboxTextureRoot, Texture::TextureColorSpace::sRGB);
			if (m_skyTexture)
			{
				Texture::TextureParams cubemapParams = m_skyTexture->GetTextureParams();
				cubemapParams.m_texFormat = Texture::TextureFormat::RGBA8;
				m_skyTexture->SetTextureParams(cubemapParams);
			}			
		}

		if (m_skyTexture != nullptr)
		{
			m_skyTexture->Create();
			LOG("Successfully loaded skybox");			
		}
		else
		{
			LOG_WARNING("Scene has no skybox");
			return;
		}

		// Create a skybox shader, now that we have some sort of image loaded:
		m_skyboxStage.GetStageShader() = 
			make_shared<Shader>(CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("skyboxShaderName"));
		m_skyboxStage.GetStageShader()->Create();

		// Set unchanging shader uniforms:
		const int xRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes");
		const int yRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes");
		const vec4 screenParams = vec4(xRes, yRes, 1.0f / xRes, 1.0f / yRes);
		m_skyboxStage.GetStageShader()->SetUniform(
			"screenParams",
			&screenParams.x,
			platform::Shader::UniformType::Vec4f,
			1);

		// Create a quad at furthest point in the depth buffer		
		m_skyMesh.emplace_back(gr::meshfactory::CreateQuad
		(
			vec3(-1.0f, 1.0f, 1.0f), // z == 1.0f, since we're in clip space (and camera's negative Z has been reversed)
			vec3(1.0f, 1.0f, 1.0f),
			vec3(-1.0f, -1.0f, 1.0f),
			vec3(1.0f, -1.0f, 1.0f)
		)); // TODO: Simplify this interface
		m_skyMesh.back()->Name() = "SkyboxQuad";

		RenderStage::RenderStageParams skyboxStageParams;
		skyboxStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
		skyboxStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Back;
		skyboxStageParams.m_srcBlendMode	= platform::Context::BlendMode::Disabled; // Render on top of the frame
		skyboxStageParams.m_dstBlendMode	= platform::Context::BlendMode::Disabled;
		skyboxStageParams.m_depthMode		= platform::Context::DepthMode::Less;
		skyboxStageParams.m_stageType		= RenderStage::RenderStageType::ColorOnly;

		m_skyboxStage.SetStageParams(skyboxStageParams);

		m_skyboxStage.GetStageCamera() = CoreEngine::GetSceneManager()->GetMainCamera();

		shared_ptr<DeferredLightingGraphicsSystem> deferredLightGS = dynamic_pointer_cast<DeferredLightingGraphicsSystem>(
			CoreEngine::GetRenderManager()->GetGraphicsSystem<DeferredLightingGraphicsSystem>());

		m_skyboxStage.GetTextureTargetSet() = deferredLightGS->GetFinalTextureTargetSet();

		pipeline.AppendRenderStage(m_skyboxStage);
	}


	void SkyboxGraphicsSystem::PreRender()
	{
		m_skyboxStage.InitializeForNewFrame();
		m_skyboxStage.SetGeometryBatches(&m_skyMesh);

		// Skybox texture can be null if we didn't load anything, but this GS should have been removed
		m_skyboxStage.SetTextureInput(
			"CubeMap0",
			m_skyTexture,
			Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear));

		shared_ptr<GBufferGraphicsSystem> gBufferGS = dynamic_pointer_cast<GBufferGraphicsSystem>(
			CoreEngine::GetRenderManager()->GetGraphicsSystem<GBufferGraphicsSystem>());

		m_skyboxStage.SetTextureInput(
			"GBufferDepth",
			gBufferGS->GetFinalTextureTargetSet().DepthStencilTarget().GetTexture(),
			Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear));

		m_skyboxStage.SetPerFrameShaderUniformByValue(
			"in_inverse_vp",
			glm::inverse(m_skyboxStage.GetStageCamera()->GetViewProjectionMatrix()),
			platform::Shader::UniformType::Matrix4x4f,
			1);
	}
}