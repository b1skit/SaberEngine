// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "Texture.h"
#include "TextureTarget.h"

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


namespace gr
{
	SkyboxGraphicsSystem::SkyboxGraphicsSystem(std::string name)
		: GraphicsSystem(name)
		, NamedObject(name)
		, m_skyboxStage("Skybox stage")
		, m_skyTexture(nullptr)
	{
		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Far);
	}


	void SkyboxGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		// Create a skybox shader, now that we have some sort of image loaded:
		m_skyboxStage.SetStageShader(re::Shader::Create(Config::Get()->GetValue<string>("skyboxShaderName")));

		// Load the HDR image:
		m_skyTexture = SceneManager::GetSceneData()->GetIBLTexture();
		m_skyTextureShaderName = "Tex0";

		gr::PipelineState skyboxStageParams;
		skyboxStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
		skyboxStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		skyboxStageParams.SetSrcBlendMode(gr::PipelineState::BlendMode::Disabled); // Render on top of the frame
		skyboxStageParams.SetDstBlendMode(gr::PipelineState::BlendMode::Disabled);
		skyboxStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::LEqual);
		skyboxStageParams.SetDepthWriteMode(gr::PipelineState::DepthWriteMode::Disabled);

		m_skyboxStage.SetStagePipelineState(skyboxStageParams);

		m_skyboxStage.AddPermanentParameterBlock(SceneManager::GetSceneData()->GetMainCamera()->GetCameraParams());

		DeferredLightingGraphicsSystem* deferredLightGS = 
			RenderManager::Get()->GetGraphicsSystem<DeferredLightingGraphicsSystem>();

		// Create a new texture target set so we can write to the deferred lighting color targets, but attach the
		// GBuffer depth for HW depth testing
		std::shared_ptr<re::TextureTargetSet> skyboxTargets = 
			re::TextureTargetSet::Create(*deferredLightGS->GetFinalTextureTargetSet(),"Skybox Targets");

		GBufferGraphicsSystem* gBufferGS = RenderManager::Get()->GetGraphicsSystem<GBufferGraphicsSystem>();

		skyboxTargets->SetDepthStencilTarget(gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget());

		m_skyboxStage.SetTextureTargetSet(skyboxTargets);

		pipeline.AppendRenderStage(&m_skyboxStage);
	}


	void SkyboxGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();

		// Skybox texture can be null if we didn't load anything, but this GS should have been removed
		m_skyboxStage.SetPerFrameTextureInput(
			m_skyTextureShaderName,
			m_skyTexture,
			Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));

		GBufferGraphicsSystem* gBufferGS = RenderManager::Get()->GetGraphicsSystem<GBufferGraphicsSystem>();
	}


	std::shared_ptr<re::TextureTargetSet const> SkyboxGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_skyboxStage.GetTextureTargetSet();
	}


	void SkyboxGraphicsSystem::CreateBatches()
	{
		const Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);
		m_skyboxStage.AddBatch(fullscreenQuadBatch);
	}
}