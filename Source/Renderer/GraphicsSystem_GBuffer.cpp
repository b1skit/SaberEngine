// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "GraphicsSystem_GBuffer.h"
#include "RenderManager.h"
#include "RenderObjectIDs.h"
#include "Stage.h"
#include "Texture.h"

#include "Core/Config.h"


namespace gr
{
	GBufferGraphicsSystem::GBufferGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_owningPipeline(nullptr)
		, m_viewBatches(nullptr)
		, m_allBatches(nullptr)
	{
		m_gBufferStage = gr::Stage::CreateGraphicsStage("GBuffer Stage", {});

		m_gBufferStage->SetBatchFilterMaskBit(
			re::Batch::Filter::AlphaBlended, gr::Stage::FilterMode::Exclude, true);

		m_gBufferStage->AddDrawStyleBits(effect::drawstyle::RenderPath_Deferred);
	}


	void GBufferGraphicsSystem::InitPipeline(
		gr::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const&,
		DataDependencies const& dataDependencies)
	{
		m_owningPipeline = &pipeline;

		// Create GBuffer color targets:
		const re::Texture::TextureParams RGBA8Params{
			.m_width = util::CheckedCast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey)),
			.m_height = util::CheckedCast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey)),
			.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc),
			.m_dimension = re::Texture::Dimension::Texture2D,
			.m_format = re::Texture::Format::RGBA8_UNORM,
			.m_colorSpace = re::Texture::ColorSpace::Linear,
			.m_mipMode = re::Texture::MipMode::None,
		};

		re::Texture::TextureParams R8Params = RGBA8Params;
		R8Params.m_format = re::Texture::Format::R8_UINT;

		// World normal may have negative components, emissive values may be > 1
		re::Texture::TextureParams RGBA16Params = RGBA8Params;
		RGBA16Params.m_format = re::Texture::Format::RGBA16F; 

		m_gBufferTargets = re::TextureTargetSet::Create("GBuffer Target Set");

		m_gBufferTargets->SetColorTarget(
			GBufferAlbedo, re::Texture::Create(GBufferTexNameHashKeys[GBufferAlbedo].GetKey(), RGBA8Params));
		m_gBufferTargets->SetColorTarget(
			GBufferWNormal, re::Texture::Create(GBufferTexNameHashKeys[GBufferWNormal].GetKey(), RGBA16Params));
		m_gBufferTargets->SetColorTarget(
			GBufferRMAO, re::Texture::Create(GBufferTexNameHashKeys[GBufferRMAO].GetKey(), RGBA8Params));
		m_gBufferTargets->SetColorTarget(
			GBufferEmissive, re::Texture::Create(GBufferTexNameHashKeys[GBufferEmissive].GetKey(), RGBA16Params));
		m_gBufferTargets->SetColorTarget(
			GBufferMatProp0, re::Texture::Create(GBufferTexNameHashKeys[GBufferMatProp0].GetKey(), RGBA8Params));

		m_gBufferTargets->SetColorTarget(
			GBufferMaterialID, re::Texture::Create(GBufferTexNameHashKeys[GBufferMaterialID].GetKey(), R8Params));
		
		// Create GBuffer depth target:
		re::Texture::TextureParams depthTexParams(RGBA8Params);
		depthTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::DepthTarget | re::Texture::Usage::ColorSrc);
		depthTexParams.m_format = re::Texture::Format::Depth32F;
		depthTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		depthTexParams.m_optimizedClear.m_depthStencil.m_depth = 1.f;

		m_gBufferTargets->SetDepthStencilTarget(
			re::Texture::Create(GBufferTexNameHashKeys[GBufferDepth].GetKey(), depthTexParams));

		m_gBufferStage->SetTextureTargetSet(m_gBufferTargets);

		// Camera:		
		m_gBufferStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		// Create a clear stage for the GBuffer targets:
		std::shared_ptr<gr::ClearTargetSetStage> gbufferClearStage = 
			gr::Stage::CreateTargetSetClearStage("GBuffer target clear stage", m_gBufferTargets);
		gbufferClearStage->EnableAllColorClear();
		gbufferClearStage->EnableDepthClear(1.f);

		m_owningPipeline->AppendStage(gbufferClearStage);

		// Finally, append the GBuffer stage to the pipeline:
		m_owningPipeline->AppendStage(m_gBufferStage);

		// Cache our dependencies:
		m_viewBatches = GetDataDependency<ViewBatches>(k_viewBatchesDataInput, dataDependencies);
		m_allBatches = GetDataDependency<AllBatches>(k_allBatchesDataInput, dataDependencies);
		SEAssert(m_viewBatches || m_allBatches, "Must have received some batches");
	}


	void GBufferGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_viewBatchesDataInput);
		RegisterDataInput(k_allBatchesDataInput);
	}


	void GBufferGraphicsSystem::RegisterOutputs()
	{
		// Color textures:
		for (uint8_t i = 0; i < GBufferColorTex_Count; i++)
		{
			RegisterTextureOutput(
				GBufferTexNameHashKeys[i], &m_gBufferTargets->GetColorTarget(i).GetTexture());
		}
		// Depth texture:
		RegisterTextureOutput(
			GBufferTexNameHashKeys[GBufferDepth], &m_gBufferTargets->GetDepthStencilTarget().GetTexture());
	}


	void GBufferGraphicsSystem::PreRender()
	{
		CreateBatches();

		if (m_gBufferStage->GetStageBatches().empty())
		{
			// Append a clear stage, to ensure that the depth buffer is cleared when there is no batches (i.e. so the 
			// skybox will still render in an empty scene)		
			std::shared_ptr<gr::ClearTargetSetStage> gbufferClearStage =
				gr::Stage::CreateSingleFrameTargetSetClearStage("GBuffer empty batches clear stage", m_gBufferTargets);
			gbufferClearStage->EnableDepthClear(1.f);

			m_owningPipeline->AppendSingleFrameStage(gbufferClearStage);
		}
	}


	void GBufferGraphicsSystem::CreateBatches()
	{
		const gr::RenderDataID mainCamID = m_graphicsSystemManager->GetActiveCameraRenderDataID();
		
		if (m_viewBatches && mainCamID != gr::k_invalidRenderDataID)
		{
			SEAssert(m_viewBatches->contains(mainCamID), "Cannot find main camera ID in view batches");

			m_gBufferStage->AddBatches(m_viewBatches->at(mainCamID));			
		}
		else
		{
			SEAssert(m_allBatches, "Must have all batches if view batches is null");

			m_gBufferStage->AddBatches(*m_allBatches);
		}
	}
}