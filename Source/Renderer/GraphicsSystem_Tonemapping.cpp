// © 2022 Adam Badke. All rights reserved.
#include "BatchBuilder.h"
#include "GraphicsSystem_Tonemapping.h"
#include "GraphicsSystemManager.h"
#include "GraphicsUtils.h"
#include "Sampler.h"
#include "Texture.h"
#include "TextureView.h"

#include "Core/InvPtr.h"

#include "Core/Util/ImGuiUtils.h"


namespace
{
	static const EffectID k_tonemappingEffectID = effect::Effect::ComputeEffectID("Tonemapping");
}

namespace gr
{
	TonemappingGraphicsSystem::TonemappingGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_currentMode(TonemappingMode::ACES)
	{
	}


	void TonemappingGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_tonemappingTargetInput);
		RegisterTextureInput(k_bloomResultInput, GraphicsSystem::TextureInputDefault::OpaqueBlack);
	}


	void TonemappingGraphicsSystem::RegisterOutputs()
	{
		//
	}


	void TonemappingGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		m_tonemappingStage = re::Stage::CreateComputeStage("Tonemapping stage", re::Stage::ComputeStageParams{});

		m_tonemappingStage->AddDrawStyleBits(effect::drawstyle::Tonemapping_ACES);

		// Buffers:
		m_tonemappingStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		// Texture inputs:
		constexpr char const* k_tonemappingTargetShaderName = "Lighting";
		constexpr char const* k_bloomShaderName = "Bloom";

		core::InvPtr<re::Texture> const& lightingTex = *texDependencies.at(k_tonemappingTargetInput);

		m_tonemappingStage->AddPermanentRWTextureInput(
			k_tonemappingTargetShaderName,
			lightingTex,
			re::TextureView(lightingTex));

		m_tonemappingStage->AddPermanentTextureInput(
			k_bloomShaderName,
			*texDependencies.at(k_bloomResultInput),
			re::Sampler::GetSampler("ClampMinMagMipLinear"),
			re::TextureView(*texDependencies.at(k_bloomResultInput)));

		pipeline.AppendStage(m_tonemappingStage);

		// Create a permanent compute batch:
		const uint32_t roundedXDim = grutil::GetRoundedDispatchDimension(lightingTex->Width(), k_dispatchXYThreadDims);
		const uint32_t roundedYDim = grutil::GetRoundedDispatchDimension(lightingTex->Height(), k_dispatchXYThreadDims);

		m_tonemappingComputeBatch = gr::ComputeBatchBuilder()
			.SetThreadGroupCount(glm::uvec3(roundedXDim, roundedYDim, 1))
			.SetEffectID(k_tonemappingEffectID)
			.Build();

		// Swap chain blit: Must handle this manually as a copy stage has limited format support
		re::Stage::FullscreenQuadParams swapchainBlitStageParams{};
		swapchainBlitStageParams.m_effectID = k_tonemappingEffectID;
		swapchainBlitStageParams.m_drawStyleBitmask = effect::drawstyle::Tonemapping_SwapchainBlit;

		m_swapchainBlitStage = re::Stage::CreateFullscreenQuadStage("Swapchain blit stage", swapchainBlitStageParams);

		m_swapchainBlitStage->SetTextureTargetSet(nullptr); // Write directly to the swapchain backbuffer

		// Texture inputs:
		m_swapchainBlitStage->AddPermanentTextureInput(
			"Tex0",
			lightingTex,
			re::Sampler::GetSampler("ClampMinMagMipLinear"),
			re::TextureView(lightingTex));

		pipeline.AppendStage(m_swapchainBlitStage);
	}


	void TonemappingGraphicsSystem::PreRender()
	{
		if (m_currentMode != TonemappingMode::PassThrough)
		{
			m_tonemappingStage->AddBatch(m_tonemappingComputeBatch);
		}
	}


	void TonemappingGraphicsSystem::ShowImGuiWindow()
	{
		constexpr char const* k_tonemappingModes[TonemappingMode::Count] = {
			ENUM_TO_STR(ACES),
			ENUM_TO_STR(ACES_FAST),
			ENUM_TO_STR(Reinhard),
			ENUM_TO_STR(PassThrough),
		};
		SEStaticAssert(TonemappingMode::Count == 4, "Number of tonemapping modes has changed this must be updated");

		constexpr char const* k_comboTitle = "Tonemapping mode";

		if (util::ShowBasicComboBox(k_comboTitle, k_tonemappingModes, TonemappingMode::Count, m_currentMode))
		{
			m_tonemappingStage->ClearDrawStyleBits();

			switch (m_currentMode)
			{
			case TonemappingMode::ACES: m_tonemappingStage->AddDrawStyleBits(effect::drawstyle::Tonemapping_ACES); break;
			case TonemappingMode::ACES_FAST: m_tonemappingStage->AddDrawStyleBits(effect::drawstyle::Tonemapping_ACES_Fast); break;
			case TonemappingMode::Reinhard: m_tonemappingStage->AddDrawStyleBits(effect::drawstyle::Tonemapping_Reinhard); break;
			case TonemappingMode::PassThrough: /*Do nothing*/ break;
			default: SEAssertF("Invalid tonemapping mode");
			}
		}
	}
}