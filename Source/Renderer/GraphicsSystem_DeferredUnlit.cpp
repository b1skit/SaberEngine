// © 2025 Adam Badke. All rights reserved.
#include "BatchBuilder.h"
#include "Effect.h"
#include "GraphicsSystem_DeferredUnlit.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystemManager.h"
#include "GraphicsUtils.h"
#include "Sampler.h"
#include "Stage.h"
#include "TextureTarget.h"
#include "TextureView.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Util/CHashKey.h"
#include "Core/Util/HashKey.h"


namespace
{
	static const EffectID k_deferredLightingEffectID = effect::Effect::ComputeEffectID("DeferredLighting");

	static const util::HashKey k_sampler2DShadowName("BorderCmpMinMagLinearMipPoint");
	static const util::HashKey k_samplerCubeShadowName("WrapCmpMinMagLinearMipPoint");

	static constexpr char const* k_directionalShadowShaderName = "DirectionalShadows";
	static constexpr char const* k_pointShadowShaderName = "PointShadows";
	static constexpr char const* k_spotShadowShaderName = "SpotShadows";


	re::TextureView CreateShadowArrayReadView(core::InvPtr<re::Texture> const& shadowArray)
	{
		return re::TextureView(
			shadowArray,
			{ re::TextureView::ViewFlags::ReadOnlyDepth });
	}
}


namespace gr
{
	DeferredUnlitGraphicsSystem::DeferredUnlitGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
	{
		m_primaryLightingTargetSet = re::TextureTargetSet::Create("Primary lighting targets");
	}


	void DeferredUnlitGraphicsSystem::RegisterInputs()
	{
		for (uint8_t slot = 0; slot < GBufferGraphicsSystem::GBufferTexIdx_Count; slot++)
		{
			if (slot == GBufferGraphicsSystem::GBufferEmissive)
			{
				continue;
			}
			RegisterTextureInput(GBufferGraphicsSystem::GBufferTexNameHashKeys[slot]);
		}
	}


	void DeferredUnlitGraphicsSystem::RegisterOutputs()
	{
		RegisterTextureOutput(k_lightingTargetTexOutput, &m_primaryLightingTargetSet->GetColorTarget(0).GetTexture());
	}


	void DeferredUnlitGraphicsSystem::InitPipeline(
		gr::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const& bufferDependencies,
		DataDependencies const& dataDependencies)
	{
		m_fullscreenStage = gr::Stage::CreateComputeStage("Deferred Unlit stage", gr::Stage::ComputeStageParams{});

		// Create a lighting texture target:
		core::InvPtr<re::Texture> const& lightTargetTex = re::Texture::Create(
			"PrimaryLightingTarget",
			re::Texture::TextureParams{
				.m_width = static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_windowWidthKey)),
				.m_height = static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_windowHeightKey)),
				.m_usage = re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = re::Texture::Format::RGBA16F,
				.m_colorSpace = re::Texture::ColorSpace::Linear,
				.m_mipMode = re::Texture::MipMode::None,
			});

		// Create the lighting target set:
		m_primaryLightingTargetSet->SetColorTarget(
			0,
			lightTargetTex,
			re::TextureTarget::TargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1) });

		// We need the depth buffer attached, but with depth writes disabled:
		m_primaryLightingTargetSet->SetDepthStencilTarget(
			*GetDependency<core::InvPtr<re::Texture>>(
				GBufferGraphicsSystem::GBufferTexNameHashKeys[GBufferGraphicsSystem::GBufferDepth], texDependencies),
			re::TextureTarget::TargetParams{ .m_textureView = {
				re::TextureView::Texture2DView(0, 1),
				{re::TextureView::ViewFlags::ReadOnlyDepth} } });

		// Append a color-only clear stage to clear the lighting target:
		std::shared_ptr<gr::ClearTargetSetStage> clearStage = gr::Stage::CreateTargetSetClearStage(
			"DeferredLighting: Clear lighting targets", m_primaryLightingTargetSet);
		clearStage->EnableAllColorClear();

		pipeline.AppendStage(clearStage);


		// Fullscreen stage:
		//------------------
		m_fullscreenStage->AddPermanentRWTextureInput(
			"LightingTarget",
			m_primaryLightingTargetSet->GetColorTarget(0).GetTexture(),
			re::TextureView(m_primaryLightingTargetSet->GetColorTarget(0).GetTexture()));

		m_fullscreenStage->AddPermanentBuffer(m_primaryLightingTargetSet->GetCreateTargetParamsBuffer());

		m_fullscreenStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_Fullscreen);

		pipeline.AppendStage(m_fullscreenStage);

		// Construct a permanent compute batch for the fullscreen stage:
		const uint32_t roundedXDim =
			grutil::GetRoundedDispatchDimension(m_primaryLightingTargetSet->GetViewport().Width(), k_dispatchXYThreadDims);
		const uint32_t roundedYDim =
			grutil::GetRoundedDispatchDimension(m_primaryLightingTargetSet->GetViewport().Height(), k_dispatchXYThreadDims);

		m_fullscreenComputeBatch = gr::ComputeBatchBuilder()
			.SetThreadGroupCount(glm::uvec3(roundedXDim, roundedYDim, 1u))
			.SetEffectID(k_deferredLightingEffectID)
			.Build();


		// Attach GBuffer inputs:
		core::InvPtr<re::Sampler> const& wrapMinMagLinearMipPoint = m_graphicsSystemManager->GetSampler("WrapMinMagLinearMipPoint");

		for (uint8_t slot = 0; slot < GBufferGraphicsSystem::GBufferTexIdx::GBufferTexIdx_Count; slot++)
		{
			if (slot == GBufferGraphicsSystem::GBufferEmissive)
			{
				continue; // The emissive texture is not used
			}

			SEAssert(texDependencies.contains(GBufferGraphicsSystem::GBufferTexNameHashKeys[slot]),
				"Texture dependency not found");

			util::CHashKey const& texName = GBufferGraphicsSystem::GBufferTexNameHashKeys[slot];
			core::InvPtr<re::Texture> const& gbufferTex = 
				*GetDependency<core::InvPtr<re::Texture>>(texName, texDependencies);

			m_fullscreenStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, re::TextureView(gbufferTex));
		}
	}


	void DeferredUnlitGraphicsSystem::PreRender()
	{
		m_fullscreenStage->AddBatch(m_fullscreenComputeBatch);
	}
}