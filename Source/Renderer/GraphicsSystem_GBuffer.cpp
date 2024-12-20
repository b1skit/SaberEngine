// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "GraphicsSystem_GBuffer.h"
#include "RenderManager.h"
#include "RenderObjectIDs.h"
#include "RenderStage.h"
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
		m_gBufferStage = re::RenderStage::CreateGraphicsStage("GBuffer Stage", {});

		m_gBufferStage->SetBatchFilterMaskBit(
			re::Batch::Filter::AlphaBlended, re::RenderStage::FilterMode::Exclude, true);

		m_gBufferStage->SetDrawStyle(effect::drawstyle::RenderPath_Deferred);
	}


	void GBufferGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const&,
		DataDependencies const& dataDependencies)
	{
		m_owningPipeline = &pipeline;

		// Create GBuffer color targets:
		re::Texture::TextureParams gBufferColorParams;
		gBufferColorParams.m_width = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		gBufferColorParams.m_height = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
		gBufferColorParams.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc);
		gBufferColorParams.m_dimension = re::Texture::Dimension::Texture2D;
		gBufferColorParams.m_format = re::Texture::Format::RGBA8_UNORM;
		gBufferColorParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		gBufferColorParams.m_mipMode = re::Texture::MipMode::None;

		// World normal may have negative components, emissive values may be > 1
		re::Texture::TextureParams gbuffer16bitParams = gBufferColorParams;
		gbuffer16bitParams.m_format = re::Texture::Format::RGBA16F; 
		gbuffer16bitParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		re::TextureTarget::TargetParams defaultTargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1)};

		m_gBufferTargets = re::TextureTargetSet::Create("GBuffer Target Set");
		for (uint8_t i = GBufferTexIdx::GBufferAlbedo; i <= GBufferTexIdx::GBufferMatProp0; i++)
		{
			if (i == GBufferWNormal || i == GBufferEmissive)
			{
				m_gBufferTargets->SetColorTarget(
					i, re::Texture::Create(GBufferTexNameHashKeys[i].GetKey(), gbuffer16bitParams), defaultTargetParams);
			}
			else
			{
				m_gBufferTargets->SetColorTarget(
					i, re::Texture::Create(GBufferTexNameHashKeys[i].GetKey(), gBufferColorParams), defaultTargetParams);
			}
		}
		
		// Create GBuffer depth target:
		re::Texture::TextureParams depthTexParams(gBufferColorParams);
		depthTexParams.m_usage = static_cast<re::Texture::Usage>(
			re::Texture::Usage::DepthTarget | re::Texture::Usage::ColorSrc);
		depthTexParams.m_format = re::Texture::Format::Depth32F;
		depthTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		depthTexParams.m_clear.m_depthStencil.m_depth = 1.f; // Far plane

		m_gBufferTargets->SetDepthStencilTarget(
			re::Texture::Create(GBufferTexNameHashKeys[GBufferTexIdx::GBufferDepth].GetKey(), depthTexParams),
			re::TextureTarget::TargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1) });

		m_gBufferStage->SetTextureTargetSet(m_gBufferTargets);

		// Camera:		
		m_gBufferStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());


		// Create a clear stage for the GBuffer targets:
		re::RenderStage::ClearStageParams gbufferClearParams; // Clear both color and depth
		gbufferClearParams.m_colorClearModes = { re::TextureTarget::ClearMode::Enabled };
		gbufferClearParams.m_depthClearMode = re::TextureTarget::ClearMode::Enabled;
		m_owningPipeline->AppendRenderStage(re::RenderStage::CreateClearStage(gbufferClearParams, m_gBufferTargets));

		// Finally, append the GBuffer stage to the pipeline:
		m_owningPipeline->AppendRenderStage(m_gBufferStage);

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
			re::RenderStage::ClearStageParams depthClearStageParams;
			depthClearStageParams.m_colorClearModes = { re::TextureTarget::ClearMode::Disabled };
			depthClearStageParams.m_depthClearMode = re::TextureTarget::ClearMode::Enabled;
			
			m_owningPipeline->AppendSingleFrameRenderStage(re::RenderStage::CreateSingleFrameClearStage(
				depthClearStageParams, 
				m_gBufferTargets));
		}
	}


	void GBufferGraphicsSystem::CreateBatches()
	{
		if (m_viewBatches)
		{
			const gr::RenderDataID mainCamID = m_graphicsSystemManager->GetActiveCameraRenderDataID();

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