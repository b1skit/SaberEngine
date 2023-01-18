// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "Texture.h"
#include "TextureTarget.h"


namespace gr
{
	using en::Config;
	using en::SceneManager;
	using gr::DeferredLightingGraphicsSystem;
	using gr::GBufferGraphicsSystem;
	using re::RenderManager;
	using re::RenderStage;
	using re::Sampler;
	using re::Batch;
	using re::TextureTargetSet;
	using re::Texture;
	using re::Shader;
	using std::shared_ptr;
	using std::string;
	using std::vector;
	using std::filesystem::exists;
	using glm::vec3;
	using glm::vec4;
	using glm::mat4;


	SkyboxGraphicsSystem::SkyboxGraphicsSystem(std::string name) : GraphicsSystem(name), NamedObject(name),
		m_skyboxStage("Skybox stage"),
		m_skyTexture(nullptr)
	{
		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Far);
	}


	void SkyboxGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		// Create a skybox shader, now that we have some sort of image loaded:
		m_skyboxStage.GetStageShader() = make_shared<Shader>(Config::Get()->GetValue<string>("skyboxShaderName"));

		// Load the HDR image:
		m_skyTexture = SceneManager::GetSceneData()->GetIBLTexture();
		m_skyTextureShaderName = "Tex0";

		RenderStage::PipelineStateParams skyboxStageParams;
		skyboxStageParams.m_targetClearMode = re::Context::ClearTarget::None;
		skyboxStageParams.m_faceCullingMode = re::Context::FaceCullingMode::Back;
		skyboxStageParams.m_srcBlendMode	= re::Context::BlendMode::Disabled; // Render on top of the frame
		skyboxStageParams.m_dstBlendMode	= re::Context::BlendMode::Disabled;
		skyboxStageParams.m_depthTestMode	= re::Context::DepthTestMode::LEqual;
		skyboxStageParams.m_depthWriteMode	= re::Context::DepthWriteMode::Disabled;

		m_skyboxStage.SetStagePipelineStateParams(skyboxStageParams);

		m_skyboxStage.SetStageCamera(SceneManager::GetSceneData()->GetMainCamera().get());

		shared_ptr<DeferredLightingGraphicsSystem> deferredLightGS = dynamic_pointer_cast<DeferredLightingGraphicsSystem>(
			RenderManager::Get()->GetGraphicsSystem<DeferredLightingGraphicsSystem>());

		// Need to create a new texture target set, so we can write to the deferred lighting color targets, but use the
		// GBuffer depth buffer for HW depth testing
		m_skyboxStage.SetTextureTargetSet(std::make_shared<re::TextureTargetSet>(
			*deferredLightGS->GetFinalTextureTargetSet(), 
			"Skybox Target Set"));

		shared_ptr<GBufferGraphicsSystem> gBufferGS = std::dynamic_pointer_cast<GBufferGraphicsSystem>(
			RenderManager::Get()->GetGraphicsSystem<GBufferGraphicsSystem>());
		SEAssert("GBuffer GS not found", gBufferGS != nullptr);
		m_skyboxStage.GetTextureTargetSet()->SetDepthStencilTarget(
			gBufferGS->GetFinalTextureTargetSet()->DepthStencilTarget());

		pipeline.AppendRenderStage(m_skyboxStage);
	}


	void SkyboxGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();

		// Skybox texture can be null if we didn't load anything, but this GS should have been removed
		m_skyboxStage.SetTextureInput(
			m_skyTextureShaderName,
			m_skyTexture,
			Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));

		shared_ptr<GBufferGraphicsSystem> gBufferGS = dynamic_pointer_cast<GBufferGraphicsSystem>(
			RenderManager::Get()->GetGraphicsSystem<GBufferGraphicsSystem>());
	}


	void SkyboxGraphicsSystem::CreateBatches()
	{
		const Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr, nullptr);
		m_skyboxStage.AddBatch(fullscreenQuadBatch);
	}


	std::shared_ptr<re::TextureTargetSet> SkyboxGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_skyboxStage.GetTextureTargetSet();
	}
}