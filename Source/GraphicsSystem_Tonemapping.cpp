// © 2022 Adam Badke. All rights reserved.
#include "Core\Definitions\ConfigKeys.h"
#include "GraphicsSystem_Tonemapping.h"
#include "GraphicsSystemManager.h"
#include "Sampler.h"
#include "Shader.h"


namespace gr
{
	constexpr char const* k_gsName = "Tone Mapping Graphics System";


	TonemappingGraphicsSystem::TonemappingGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, NamedObject(k_gsName)
	{
		m_tonemappingStage = 
			re::RenderStage::CreateFullscreenQuadStage("Tonemapping stage", re::RenderStage::FullscreenQuadParams{});
	}


	void TonemappingGraphicsSystem::InitPipeline(re::StagePipeline& pipeline, TextureDependencies const& texDependencies)
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
		m_tonemappingStage->AddTextureInput(
			"Tex0",
			texDependencies.at(k_tonemappingTargetInput),
			re::Sampler::GetSampler("ClampMinMagMipLinear"));

		m_tonemappingStage->AddTextureInput(
			"Tex1",
			texDependencies.at(k_bloomResultInput),
			re::Sampler::GetSampler("ClampMinMagMipLinear"));

		pipeline.AppendRenderStage(m_tonemappingStage);
	}


	void TonemappingGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_tonemappingTargetInput);
		RegisterTextureInput(k_bloomResultInput);
	}


	void TonemappingGraphicsSystem::RegisterOutputs()
	{
		//
	}


	void TonemappingGraphicsSystem::PreRender(DataDependencies const&)
	{
		//
	}
}