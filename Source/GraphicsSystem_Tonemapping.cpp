// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Tonemapping.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "RenderManager.h"
#include "RenderSystem.h"
#include "Shader.h"
#include "SceneManager.h"


using en::Config;
using en::SceneManager;
using re::Shader;
using gr::DeferredLightingGraphicsSystem;
using re::TextureTargetSet;
using re::RenderManager;
using re::RenderStage;
using re::Batch;
using re::Sampler;
using std::shared_ptr;
using std::make_shared;
using std::string;
using glm::vec3;


namespace gr
{
	constexpr char const* k_gsName = "Tone Mapping Graphics System";


	TonemappingGraphicsSystem::TonemappingGraphicsSystem()
		: GraphicsSystem(k_gsName)
		, NamedObject(k_gsName)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_tonemappingStage = re::RenderStage::CreateGraphicsStage("Tonemapping stage", gfxStageParams);

		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Near);
	}


	void TonemappingGraphicsSystem::Create(re::RenderSystem& renderSystem, re::StagePipeline& pipeline)
	{
		gr::PipelineState tonemappingStageParam;
		tonemappingStageParam.SetClearTarget(gr::PipelineState::ClearTarget::None);
		tonemappingStageParam.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		tonemappingStageParam.SetDepthTestMode(gr::PipelineState::DepthTestMode::Always);

		m_tonemappingStage->SetStagePipelineState(tonemappingStageParam);

		m_tonemappingStage->SetStageShader(re::Shader::Create(en::ShaderNames::k_toneMapShaderName));

		m_tonemappingStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer

		// Camera params:
		m_tonemappingStage->AddPermanentParameterBlock(SceneManager::Get()->GetMainCamera()->GetCameraParams());

		std::shared_ptr<TextureTargetSet const> deferredLightTextureTargetSet =
			renderSystem.GetGraphicsSystem<DeferredLightingGraphicsSystem>()->GetFinalTextureTargetSet();

		m_tonemappingStage->AddTextureInput(
			"GBufferAlbedo",
			deferredLightTextureTargetSet->GetColorTarget(0).GetTexture(),
			Sampler::GetSampler(Sampler::WrapAndFilterMode::Wrap_Linear_Linear));

		pipeline.AppendRenderStage(m_tonemappingStage);
	}


	void TonemappingGraphicsSystem::PreRender()
	{
		CreateBatches();
	}


	void TonemappingGraphicsSystem::CreateBatches()
	{
		const Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);
		m_tonemappingStage->AddBatch(fullscreenQuadBatch);
	}


	std::shared_ptr<re::TextureTargetSet const> TonemappingGraphicsSystem::GetFinalTextureTargetSet() const 
	{
		return m_tonemappingStage->GetTextureTargetSet();
	}
}