// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Tonemapping.h"
#include "MeshFactory.h"
#include "RenderManager.h"
#include "RenderSystem.h"
#include "Sampler.h"
#include "Shader.h"


namespace gr
{
	constexpr char const* k_gsName = "Tone Mapping Graphics System";


	TonemappingGraphicsSystem::TonemappingGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, NamedObject(k_gsName)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_tonemappingStage = re::RenderStage::CreateGraphicsStage("Tonemapping stage", gfxStageParams);

		m_screenAlignedQuad = gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Near);
	}


	void TonemappingGraphicsSystem::InitPipeline(re::StagePipeline& pipeline)
	{
		re::PipelineState tonemappingPipelineState;
		tonemappingPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);
		tonemappingPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);

		m_tonemappingStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_toneMapShaderName, tonemappingPipelineState));

		m_tonemappingStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer

		// Param blocks:
		m_tonemappingStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		// Texture inputs:
		std::shared_ptr<re::TextureTargetSet const> deferredLightTextureTargetSet =
			m_graphicsSystemManager->GetGraphicsSystem<DeferredLightingGraphicsSystem>()->GetFinalTextureTargetSet();

		m_tonemappingStage->AddTextureInput(
			"Tex0",
			deferredLightTextureTargetSet->GetColorTarget(0).GetTexture(),
			re::Sampler::GetSampler("ClampMinMagMipLinear"));
		
		gr::BloomGraphicsSystem* bloomGS = m_graphicsSystemManager->GetGraphicsSystem<BloomGraphicsSystem>();
		std::shared_ptr<re::TextureTargetSet const> bloomTextureTargetSet = bloomGS->GetFinalTextureTargetSet();

		m_tonemappingStage->AddTextureInput(
			"Tex1",
			bloomTextureTargetSet->GetColorTarget(0).GetTexture(),
			re::Sampler::GetSampler("ClampMinMagMipLinear"));

		pipeline.AppendRenderStage(m_tonemappingStage);
	}


	void TonemappingGraphicsSystem::PreRender()
	{
		CreateBatches();
	}


	void TonemappingGraphicsSystem::CreateBatches()
	{
		if (m_fullscreenQuadBatch == nullptr)
		{
			m_fullscreenQuadBatch = std::make_unique<re::Batch>(re::Batch::Lifetime::Permanent, m_screenAlignedQuad.get());
		}
		m_tonemappingStage->AddBatch(*m_fullscreenQuadBatch);
	}
}