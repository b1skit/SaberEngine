// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_Tonemapping.h"
#include "MeshFactory.h"
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


	TonemappingGraphicsSystem::TonemappingGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, NamedObject(k_gsName)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_tonemappingStage = re::RenderStage::CreateGraphicsStage("Tonemapping stage", gfxStageParams);

		m_screenAlignedQuad = gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Near);
	}


	void TonemappingGraphicsSystem::Create(re::RenderSystem& renderSystem, re::StagePipeline& pipeline)
	{
		m_owningRenderSystem = &renderSystem;

		re::PipelineState tonemappingPipelineState;
		tonemappingPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);
		tonemappingPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);

		m_tonemappingStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_toneMapShaderName, tonemappingPipelineState));

		m_tonemappingStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer

		// Param blocks:
		m_tonemappingStage->AddPermanentParameterBlock(m_owningGraphicsSystemManager->GetActiveCameraParams());

		// Texture inputs:
		std::shared_ptr<TextureTargetSet const> deferredLightTextureTargetSet =
			m_owningGraphicsSystemManager->GetGraphicsSystem<DeferredLightingGraphicsSystem>()->GetFinalTextureTargetSet();

		m_tonemappingStage->AddTextureInput(
			"Tex0",
			deferredLightTextureTargetSet->GetColorTarget(0).GetTexture(),
			Sampler::GetSampler(Sampler::WrapAndFilterMode::Clamp_LinearMipMapLinear_Linear));
		
		gr::BloomGraphicsSystem* bloomGS = m_owningGraphicsSystemManager->GetGraphicsSystem<BloomGraphicsSystem>();
		std::shared_ptr<TextureTargetSet const> bloomTextureTargetSet = bloomGS->GetFinalTextureTargetSet();

		m_tonemappingStage->AddTextureInput(
			"Tex1",
			bloomTextureTargetSet->GetColorTarget(0).GetTexture(),
			Sampler::GetSampler(Sampler::WrapAndFilterMode::Clamp_LinearMipMapLinear_Linear));

		pipeline.AppendRenderStage(m_tonemappingStage);
	}


	void TonemappingGraphicsSystem::PreRender()
	{
		CreateBatches();
	}


	void TonemappingGraphicsSystem::CreateBatches()
	{
		const Batch fullscreenQuadBatch = Batch(re::Batch::Lifetime::SingleFrame, m_screenAlignedQuad.get(), nullptr);
		m_tonemappingStage->AddBatch(fullscreenQuadBatch);
	}
}