#include <memory>
#include <filesystem>

#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "CoreEngine.h"
#include "Texture.h"

using gr::Texture;
using gr::DeferredLightingGraphicsSystem;
using gr::GBufferGraphicsSystem;
using re::Batch;
using en::CoreEngine;
using std::shared_ptr;
using std::string;
using std::vector;
using std::filesystem::exists;
using glm::vec3;
using glm::vec4;
using glm::mat4;


namespace gr
{
	SkyboxGraphicsSystem::SkyboxGraphicsSystem(std::string name) : GraphicsSystem(name), NamedObject(name),
		m_skyboxStage("Skybox stage"),
		m_skyTexture(nullptr)
	{
		m_screenAlignedQuad = gr::meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Far);
	}


	void SkyboxGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		// Create a skybox shader, now that we have some sort of image loaded:
		m_skyboxStage.GetStageShader() =
			make_shared<Shader>(CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("skyboxShaderName"));

		// Load the HDR image:
		const string iblTexturePath = en::CoreEngine::GetConfig()->GetValue<string>("sceneIBLPath");
		m_skyTexture = CoreEngine::GetSceneManager()->GetSceneData()->GetLoadTextureByPath({ iblTexturePath }, false);
		if (!m_skyTexture)
		{
			const string defaultIBLPath = en::CoreEngine::GetConfig()->GetValue<string>("defaultIBLPath");
			m_skyTexture = CoreEngine::GetSceneManager()->GetSceneData()->GetLoadTextureByPath({ defaultIBLPath }, true);
		}

		if (m_skyTexture == nullptr)
		{
			const string& sceneName = CoreEngine::GetSceneManager()->GetSceneData()->GetName();
			const string skyboxTextureRoot =
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("scenesRoot") + sceneName + "\\Skybox\\";
			// TODO: This skybox path should be user-configurable



			// Assemble a list of filepaths to load:
			// TODO: WE SHOULD NOT BE EXAMINING THE FILESYSTEM FROM WITHIN A GS!!!!!!!!!!
			vector<string> cubemapTexPaths;
			const vector<string> cubemapFaceTextureNames =
			{
				"posx",
				"negx",
				"posy",
				"negy",
				"posz",
				"negz",
			};
			// Add any desired skybox texture filetype extensions here
			const vector<string> cubemapFileExtensions =
			{
				".jpg",
				".jpeg",
				".png",
				".tga",
			};

			for (size_t face = 0; face < 6; face++)
			{
				// Build a list of filenames:
				const string currentCubeFaceName = skyboxTextureRoot + cubemapFaceTextureNames[face];

				for (size_t ext = 0; ext < cubemapFileExtensions.size(); ext++)
				{
					const string finalName = currentCubeFaceName + cubemapFileExtensions[ext];
					if (exists(finalName))
					{
						cubemapTexPaths.emplace_back(finalName);
					}
				}
			}

			if (cubemapTexPaths.size() == 6)
			{
				m_skyTexture = CoreEngine::GetSceneManager()->GetSceneData()->GetLoadTextureByPath(cubemapTexPaths);
			}
			else
			{
				LOG_ERROR("Could not find a full set of skybox cubemap textures");
			}

			if (m_skyTexture)
			{
				Texture::TextureParams cubemapParams = m_skyTexture->GetTextureParams();
				cubemapParams.m_texFormat = Texture::TextureFormat::RGBA8;
				cubemapParams.m_texColorSpace = Texture::TextureColorSpace::sRGB;
				m_skyTexture->SetTextureParams(cubemapParams);

				m_skyboxStage.GetStageShader()->ShaderKeywords().emplace_back("CUBEMAP_SKY");
				m_skyTextureShaderName = "CubeMap0";
			}			
		}
		else
		{
			Texture::TextureParams iblParams = m_skyTexture->GetTextureParams();
			iblParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
			m_skyTexture->SetTextureParams(iblParams);
			m_skyTextureShaderName = "Tex0";
		}

		if (m_skyTexture != nullptr)
		{
			m_skyTexture->Create();
			m_skyboxStage.GetStageShader()->Create();
			LOG("Successfully loaded skybox");			
		}
		else
		{
			LOG_WARNING("Scene has no skybox");
			return;
		}


		RenderStage::RenderStageParams skyboxStageParams;
		skyboxStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
		skyboxStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Back;
		skyboxStageParams.m_srcBlendMode	= platform::Context::BlendMode::Disabled; // Render on top of the frame
		skyboxStageParams.m_dstBlendMode	= platform::Context::BlendMode::Disabled;
		skyboxStageParams.m_depthTestMode	= platform::Context::DepthTestMode::LEqual;
		skyboxStageParams.m_depthWriteMode	= platform::Context::DepthWriteMode::Disabled;

		m_skyboxStage.SetRenderStageParams(skyboxStageParams);

		m_skyboxStage.GetStageCamera() = CoreEngine::GetSceneManager()->GetSceneData()->GetMainCamera();

		shared_ptr<DeferredLightingGraphicsSystem> deferredLightGS = dynamic_pointer_cast<DeferredLightingGraphicsSystem>(
			CoreEngine::GetRenderManager()->GetGraphicsSystem<DeferredLightingGraphicsSystem>());

		// Need to create a new texture target set, so we can write to the deferred lighting color targets, but use the
		// GBuffer depth buffer for HW depth testing
		m_skyboxStage.GetTextureTargetSet() = TextureTargetSet(
			deferredLightGS->GetFinalTextureTargetSet(), 
			"Skybox Target Set");

		shared_ptr<GBufferGraphicsSystem> gBufferGS = std::dynamic_pointer_cast<GBufferGraphicsSystem>(
			en::CoreEngine::GetRenderManager()->GetGraphicsSystem<GBufferGraphicsSystem>());
		SEAssert("GBuffer GS not found", gBufferGS != nullptr);
		m_skyboxStage.GetTextureTargetSet().DepthStencilTarget() = gBufferGS->GetFinalTextureTargetSet().DepthStencilTarget();
		m_skyboxStage.GetTextureTargetSet().CreateDepthStencilTarget();

		pipeline.AppendRenderStage(m_skyboxStage);
	}


	void SkyboxGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		m_skyboxStage.InitializeForNewFrame();
		CreateBatches();

		// Skybox texture can be null if we didn't load anything, but this GS should have been removed
		m_skyboxStage.SetTextureInput(
			m_skyTextureShaderName,
			m_skyTexture,
			Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear));

		shared_ptr<GBufferGraphicsSystem> gBufferGS = dynamic_pointer_cast<GBufferGraphicsSystem>(
			CoreEngine::GetRenderManager()->GetGraphicsSystem<GBufferGraphicsSystem>());
	}


	void SkyboxGraphicsSystem::CreateBatches()
	{
		const Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr, nullptr);
		m_skyboxStage.AddBatch(fullscreenQuadBatch);
	}
}