// © 2022 Adam Badke. All rights reserved.
#include "GraphicsSystem_Tonemapping.h"
#include "GraphicsSystemManager.h"
#include "Sampler.h"

#include "Core/Definitions/ConfigKeys.h"


namespace
{
	static const EffectID k_tonemappingEffectID = effect::Effect::ComputeEffectID("Tonemapping");
}

namespace gr
{
	TonemappingGraphicsSystem::TonemappingGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
	{
	}


	void TonemappingGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		re::Stage::FullscreenQuadParams tonemappingStageParams{};
		tonemappingStageParams.m_effectID = k_tonemappingEffectID;

		m_tonemappingStage = re::Stage::CreateFullscreenQuadStage("Tonemapping stage", tonemappingStageParams);

		m_tonemappingStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer

		// Param blocks:
		m_tonemappingStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		// Texture inputs:
		m_tonemappingStage->AddPermanentTextureInput(
			"Tex0",
			*texDependencies.at(k_tonemappingTargetInput),
			re::Sampler::GetSampler("ClampMinMagMipLinear"),
			re::TextureView(*texDependencies.at(k_tonemappingTargetInput)));

		m_tonemappingStage->AddPermanentTextureInput(
			"Tex1",
			*texDependencies.at(k_bloomResultInput),
			re::Sampler::GetSampler("ClampMinMagMipLinear"),
			re::TextureView(*texDependencies.at(k_bloomResultInput)));

		pipeline.AppendStage(m_tonemappingStage);
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


	void TonemappingGraphicsSystem::PreRender()
	{
		//
	}
}