// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "Core\Config.h"
#include "GraphicsSystem_GBuffer.h"
#include "RenderManager.h"
#include "RenderObjectIDs.h"
#include "RenderStage.h"
#include "Shader.h"
#include "Texture.h"


namespace gr
{
	constexpr char const* k_gsName = "GBuffer Graphics System";


	GBufferGraphicsSystem::GBufferGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: INamedObject(k_gsName)
		, GraphicsSystem(k_gsName, owningGSM)
		, m_owningPipeline(nullptr)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_gBufferStage = re::RenderStage::CreateGraphicsStage("GBuffer Stage", gfxStageParams);

		m_gBufferStage->SetBatchFilterMaskBit(re::Batch::Filter::AlphaBlended);
	}


	void GBufferGraphicsSystem::InitPipeline(re::StagePipeline& pipeline, TextureDependencies const& texDependencies)
	{
		m_owningPipeline = &pipeline;

		re::PipelineState gBufferPipelineState;
		gBufferPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);
		gBufferPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Less);

		std::shared_ptr<re::Shader> gBufferShader = re::Shader::GetOrCreate(
			{
				{"GBuffer_VShader", re::Shader::Vertex},
				{"GBuffer_PShader", re::Shader::Pixel}
			},
			gBufferPipelineState);

		m_gBufferStage->SetStageShader(gBufferShader);

		// Create GBuffer color targets:
		re::Texture::TextureParams gBufferColorParams;
		gBufferColorParams.m_width = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		gBufferColorParams.m_height = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
		gBufferColorParams.m_faces = 1;
		gBufferColorParams.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::Color);
		gBufferColorParams.m_dimension = re::Texture::Dimension::Texture2D;
		gBufferColorParams.m_format = re::Texture::Format::RGBA8_UNORM;
		gBufferColorParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		gBufferColorParams.m_addToSceneData = false;
		gBufferColorParams.m_mipMode = re::Texture::MipMode::None;

		// World normal may have negative components, emissive values may be > 1
		re::Texture::TextureParams gbuffer16bitParams = gBufferColorParams;
		gbuffer16bitParams.m_format = re::Texture::Format::RGBA16F; 
		gbuffer16bitParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		re::TextureTarget::TargetParams defaultTargetParams;

		std::shared_ptr<re::TextureTargetSet> gBufferTargets = re::TextureTargetSet::Create("GBuffer Target Set");
		for (uint8_t i = GBufferTexIdx::GBufferAlbedo; i <= GBufferTexIdx::GBufferMatProp0; i++)
		{
			if (i == GBufferWNormal || i == GBufferEmissive)
			{
				gBufferTargets->SetColorTarget(
					i, re::Texture::Create(GBufferTexNames[i], gbuffer16bitParams), defaultTargetParams);
			}
			else
			{
				gBufferTargets->SetColorTarget(
					i, re::Texture::Create(GBufferTexNames[i], gBufferColorParams), defaultTargetParams);
			}
		}
		
		// Create GBuffer depth target:
		re::Texture::TextureParams depthTexParams(gBufferColorParams);
		depthTexParams.m_usage = static_cast<re::Texture::Usage>(
			re::Texture::Usage::DepthTarget | re::Texture::Usage::Color);
		depthTexParams.m_format = re::Texture::Format::Depth32F;
		depthTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		depthTexParams.m_clear.m_depthStencil.m_depth = 1.f; // Far plane

		gBufferTargets->SetDepthStencilTarget(
			re::Texture::Create(GBufferTexNames[GBufferTexIdx::GBufferDepth], depthTexParams),
			defaultTargetParams);

		const re::TextureTarget::TargetParams::BlendModes gbufferBlendModes
		{
			re::TextureTarget::TargetParams::BlendMode::Disabled,
			re::TextureTarget::TargetParams::BlendMode::Disabled
		};
		gBufferTargets->SetColorTargetBlendModes(1, &gbufferBlendModes);

		m_gBufferStage->SetTextureTargetSet(gBufferTargets);

		// Camera:		
		m_gBufferStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());


		// Create a clear stage for the GBuffer targets:
		re::RenderStage::ClearStageParams gbufferClearParams; // Clear both color and depth
		gbufferClearParams.m_colorClearModes = { re::TextureTarget::TargetParams::ClearMode::Enabled };
		gbufferClearParams.m_depthClearMode = re::TextureTarget::TargetParams::ClearMode::Enabled;
		pipeline.AppendRenderStage(re::RenderStage::CreateClearStage(gbufferClearParams, gBufferTargets));


		// Finally, append the GBuffer stage to the pipeline:
		pipeline.AppendRenderStage(m_gBufferStage);
	}


	void GBufferGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_cullingInput);
	}


	void GBufferGraphicsSystem::RegisterOutputs()
	{
		// Color textures:
		for (uint8_t i = 0; i < GBufferColorTex_Count; i++)
		{
			RegisterTextureOutput(
				GBufferTexNames[i], m_gBufferStage->GetTextureTargetSet()->GetColorTarget(i).GetTexture());
		}
		// Depth texture:
		RegisterTextureOutput(
			GBufferTexNames[GBufferDepth], m_gBufferStage->GetTextureTargetSet()->GetDepthStencilTarget()->GetTexture());
	}


	void GBufferGraphicsSystem::PreRender(DataDependencies const& dataDependencies)
	{
		CreateBatches(dataDependencies);

		if (m_gBufferStage->GetStageBatches().empty())
		{
			// Append a clear stage, to ensure that the depth buffer is cleared when there is no batches (i.e. so the 
			// skybox will still render in an empty scene)
			re::RenderStage::ClearStageParams depthClearStageParams;
			depthClearStageParams.m_colorClearModes = { re::TextureTarget::TargetParams::ClearMode::Disabled };
			depthClearStageParams.m_depthClearMode = re::TextureTarget::TargetParams::ClearMode::Enabled;
			
			m_owningPipeline->AppendSingleFrameRenderStage(re::RenderStage::CreateSingleFrameClearStage(
				depthClearStageParams, 
				m_gBufferStage->GetTextureTargetSet()));
		}
	}


	void GBufferGraphicsSystem::CreateBatches(DataDependencies const& dataDependencies)
	{
		gr::BatchManager const& batchMgr = m_graphicsSystemManager->GetBatchManager();

		ViewCullingResults const* cullingResults = 
			static_cast<ViewCullingResults const*>(dataDependencies.at(k_cullingInput));
		
		if (cullingResults)
		{
			const gr::RenderDataID mainCamID = m_graphicsSystemManager->GetActiveCameraRenderDataID();

			m_gBufferStage->AddBatches(batchMgr.GetSceneBatches(
				m_graphicsSystemManager->GetRenderData(),
				cullingResults->at(mainCamID)));
		}
		else
		{
			m_gBufferStage->AddBatches(batchMgr.GetAllSceneBatches(m_graphicsSystemManager->GetRenderData()));
		}
	}
}