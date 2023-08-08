// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Tonemapping.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "RenderManager.h"
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


namespace
{
	struct TonemappingParams
	{
		glm::vec4 g_exposure; // .x = exposure, .yzw = unused

		static constexpr char const* const s_shaderName = "TonemappingParams"; // Not counted towards size of struct
	};

	TonemappingParams CreateTonemappingParamsData()
	{
		TonemappingParams tonemappingParams;
		tonemappingParams.g_exposure = 
			glm::vec4(SceneManager::GetSceneData()->GetMainCamera()->GetExposure(), 0.f, 0.f, 0.f);

		return tonemappingParams;
	}
}


namespace gr
{
	TonemappingGraphicsSystem::TonemappingGraphicsSystem(std::string name)
		: GraphicsSystem(name)
		, NamedObject(name)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_tonemappingStage = re::RenderStage::CreateGraphicsStage("Tonemapping stage", gfxStageParams);

		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Near);
	}


	void TonemappingGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		gr::PipelineState tonemappingStageParam;
		tonemappingStageParam.SetClearTarget(gr::PipelineState::ClearTarget::None);
		tonemappingStageParam.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		tonemappingStageParam.SetDepthTestMode(gr::PipelineState::DepthTestMode::Always);

		m_tonemappingStage->SetStagePipelineState(tonemappingStageParam);

		m_tonemappingStage->SetStageShader(re::Shader::Create(Config::Get()->GetValue<string>("toneMapShader")));

		m_tonemappingStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer

		// Tonemapping param block:
		TonemappingParams tonemappingParams = CreateTonemappingParamsData();
		shared_ptr<re::ParameterBlock> tonemappingPB = re::ParameterBlock::Create(
			TonemappingParams::s_shaderName,
			tonemappingParams,
			re::ParameterBlock::PBType::Immutable);

		m_tonemappingStage->AddPermanentParameterBlock(tonemappingPB);

		pipeline.AppendRenderStage(m_tonemappingStage);
	}


	void TonemappingGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();

		std::shared_ptr<TextureTargetSet const> deferredLightTextureTargetSet =
			RenderManager::Get()->GetGraphicsSystem<DeferredLightingGraphicsSystem>()->GetFinalTextureTargetSet();

		m_tonemappingStage->SetPerFrameTextureInput(
			"GBufferAlbedo",
			deferredLightTextureTargetSet->GetColorTarget(0).GetTexture(),
			Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
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