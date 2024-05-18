// © 2022 Adam Badke. All rights reserved.
#include "GraphicsSystem_Tonemapping.h"
#include "GraphicsSystemManager.h"
#include "Sampler.h"

#include "Core\Definitions\ConfigKeys.h"


namespace gr
{
	constexpr char const* k_gsName = "Tone Mapping Graphics System";


	TonemappingGraphicsSystem::TonemappingGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, INamedObject(k_gsName)
	{
	}


	void TonemappingGraphicsSystem::InitPipeline(re::StagePipeline& pipeline, TextureDependencies const& texDependencies)
	{
		re::RenderStage::FullscreenQuadParams tonemappingStageParams{};
		tonemappingStageParams.m_effectID = effect::Effect::ComputeEffectID("Tonemapping");

		m_tonemappingStage = re::RenderStage::CreateFullscreenQuadStage("Tonemapping stage", tonemappingStageParams);

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