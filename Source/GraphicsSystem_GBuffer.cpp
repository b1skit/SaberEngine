// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Culling.h"
#include "GraphicsSystem_GBuffer.h"
#include "RenderManager.h"
#include "SceneData.h"
#include "SceneManager.h"
#include "Shader.h"
#include "Texture.h"


using re::Shader;
using re::Texture;
using en::Config;
using fr::SceneManager;
using re::RenderManager;
using re::RenderStage;
using std::string;


namespace gr
{
	const std::array<std::string, GBufferGraphicsSystem::GBufferTexIdx_Count> GBufferGraphicsSystem::GBufferTexNames
	{
		ENUM_TO_STR(GBufferAlbedo),		// 0
		ENUM_TO_STR(GBufferWNormal),	// 1
		ENUM_TO_STR(GBufferRMAO),		// 2
		ENUM_TO_STR(GBufferEmissive),	// 3
		ENUM_TO_STR(GBufferMatProp0),	// 4
		ENUM_TO_STR(GBufferDepth),		// 5
	};
	// TODO: Split this into 2 lists: color target names, and depth names
	// -> Often need to loop over color, and treat depth differently
	
	constexpr char const* k_gsName = "GBuffer Graphics System";


	GBufferGraphicsSystem::GBufferGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: NamedObject(k_gsName)
		, GraphicsSystem(k_gsName, owningGSM)
		, m_owningPipeline(nullptr)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_gBufferStage = re::RenderStage::CreateGraphicsStage("GBuffer Stage", gfxStageParams);

		m_gBufferStage->SetBatchFilterMaskBit(re::Batch::Filter::AlphaBlended);
	}


	void GBufferGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		m_owningPipeline = &pipeline;

		re::PipelineState gBufferPipelineState;
		gBufferPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);
		gBufferPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Less);

		std::shared_ptr<Shader> gBufferShader = 
			re::Shader::GetOrCreate(en::ShaderNames::k_gbufferShaderName, gBufferPipelineState);

		m_gBufferStage->SetStageShader(gBufferShader);

		// Create GBuffer color targets:
		Texture::TextureParams gBufferColorParams;
		gBufferColorParams.m_width = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey);
		gBufferColorParams.m_height = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey);
		gBufferColorParams.m_faces = 1;
		gBufferColorParams.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::Color);
		gBufferColorParams.m_dimension = re::Texture::Dimension::Texture2D;
		gBufferColorParams.m_format = re::Texture::Format::RGBA8;
		gBufferColorParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		gBufferColorParams.m_addToSceneData = false;
		gBufferColorParams.m_mipMode = re::Texture::MipMode::None;

		// World normal may have negative components, emissive values may be > 1
		Texture::TextureParams gbuffer16bitParams = gBufferColorParams;
		gbuffer16bitParams.m_format = re::Texture::Format::RGBA16F; 
		gbuffer16bitParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		re::TextureTarget::TargetParams gbufferTargetParams;
		gbufferTargetParams.m_clearMode = re::TextureTarget::TargetParams::ClearMode::Enabled;

		std::shared_ptr<re::TextureTargetSet> gBufferTargets = re::TextureTargetSet::Create("GBuffer Target Set");
		for (uint8_t i = GBufferTexIdx::GBufferAlbedo; i <= GBufferTexIdx::GBufferMatProp0; i++)
		{
			if (i == GBufferWNormal || i == GBufferEmissive)
			{
				gBufferTargets->SetColorTarget(
					i, re::Texture::Create(GBufferTexNames[i], gbuffer16bitParams, false), gbufferTargetParams);
			}
			else
			{
				gBufferTargets->SetColorTarget(
					i, re::Texture::Create(GBufferTexNames[i], gBufferColorParams, false), gbufferTargetParams);
			}
		}
		
		// Create GBuffer depth target:
		re::Texture::TextureParams depthTexParams(gBufferColorParams);
		depthTexParams.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::DepthTarget | re::Texture::Usage::Color);
		depthTexParams.m_format = re::Texture::Format::Depth32F;
		depthTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		depthTexParams.m_clear.m_depthStencil.m_depth = 1.f; // Far plane

		re::TextureTarget::TargetParams depthTargetParams;
		depthTargetParams.m_clearMode = re::TextureTarget::TargetParams::ClearMode::Enabled;

		gBufferTargets->SetDepthStencilTarget(
			re::Texture::Create(GBufferTexNames[GBufferTexIdx::GBufferDepth], depthTexParams, false),
			depthTargetParams);

		const re::TextureTarget::TargetParams::BlendModes gbufferBlendModes
		{
			re::TextureTarget::TargetParams::BlendMode::Disabled,
			re::TextureTarget::TargetParams::BlendMode::Disabled
		};
		gBufferTargets->SetColorTargetBlendModes(1, &gbufferBlendModes);

		m_gBufferStage->SetTextureTargetSet(gBufferTargets);

		// Camera:		
		m_gBufferStage->AddPermanentParameterBlock(m_graphicsSystemManager->GetActiveCameraParams());

		// Finally, append the GBuffer stage to the pipeline:
		pipeline.AppendRenderStage(m_gBufferStage);
	}


	void GBufferGraphicsSystem::PreRender()
	{
		CreateBatches();

		if (m_gBufferStage->GetStageBatches().empty())
		{
			// Append a clear stage, to ensure that the depth buffer is cleared when there is no batches (i.e. so the 
			// skybox will still render in an empty scene)
			re::RenderStage::ClearStageParams depthClearStageParams;
			depthClearStageParams.m_colorClearModes = { re::TextureTarget::TargetParams::ClearMode::Disabled };
			depthClearStageParams.m_depthClearMode = re::TextureTarget::TargetParams::ClearMode::Enabled;
			std::vector<re::TextureTarget::TargetParams::ClearMode> color = { re::TextureTarget::TargetParams::ClearMode::Disabled };
			
			m_owningPipeline->AppendSingleFrameRenderStage(re::RenderStage::CreateSingleFrameClearStage(
				depthClearStageParams, 
				m_gBufferStage->GetTextureTargetSet()));
		}
	}


	void GBufferGraphicsSystem::CreateBatches()
	{
		const gr::RenderDataID mainCamID = m_graphicsSystemManager->GetActiveCameraRenderDataID();

		m_gBufferStage->AddBatches(m_graphicsSystemManager->GetVisibleBatches(
			gr::Camera::View(mainCamID, gr::Camera::View::Face::Default)));
	}


	std::shared_ptr<re::TextureTargetSet const> GBufferGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_gBufferStage->GetTextureTargetSet();
	}
}