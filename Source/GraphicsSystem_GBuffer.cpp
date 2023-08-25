// © 2022 Adam Badke. All rights reserved.
#include "GraphicsSystem_GBuffer.h"
#include "Config.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "Shader.h"
#include "Texture.h"
#include "SceneData.h"

using re::Shader;
using re::Texture;
using en::Config;
using en::SceneManager;
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
	

	GBufferGraphicsSystem::GBufferGraphicsSystem(std::string name)
		: NamedObject(name)
		, GraphicsSystem(name)	
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_gBufferStage = re::RenderStage::CreateGraphicsStage("GBuffer Stage", gfxStageParams);
	}


	void GBufferGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		// Shader:
		std::shared_ptr<Shader> gBufferShader = 
			re::Shader::Create(Config::Get()->GetValue<string>("gBufferFillShaderName"));

		m_gBufferStage->SetStageShader(gBufferShader);

		// Create GBuffer color targets:
		Texture::TextureParams gBufferColorParams;
		gBufferColorParams.m_width = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowXResValueName);
		gBufferColorParams.m_height = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowYResValueName);
		gBufferColorParams.m_faces = 1;
		gBufferColorParams.m_usage = re::Texture::Usage::ColorTarget;
		gBufferColorParams.m_dimension = re::Texture::Dimension::Texture2D;
		gBufferColorParams.m_format = re::Texture::Format::RGBA8;
		gBufferColorParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		gBufferColorParams.m_addToSceneData = false;

		gBufferColorParams.m_useMIPs = false;
		// TODO: Currently, our GBuffer doesn't use mipmapping, but it should.
		// We need to compute the appropriate mip level in the shader, by writing UV derivatives during the GBuffer
		// pass, and using a stencil mask to ensure we're sampling the correct material at boundaries
		// https://www.reedbeta.com/blog/deferred-texturing/
		// -> We'll also need to trigger mip generation after laying down the GBuffer

		// World normal may have negative components, emissive values may be > 1
		Texture::TextureParams gbuffer16bitParams = gBufferColorParams;
		gbuffer16bitParams.m_format = re::Texture::Format::RGBA16F; 

		re::TextureTarget::TargetParams gbufferTargetParams;
		gbufferTargetParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

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
		depthTexParams.m_usage = re::Texture::Usage::DepthTarget;
		depthTexParams.m_format = re::Texture::Format::Depth32F;
		depthTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		
		re::TextureTarget::TargetParams depthTargetParams;

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
		m_gBufferStage->AddPermanentParameterBlock(SceneManager::Get()->GetMainCamera()->GetCameraParams());

		// Set the stage params:
		gr::PipelineState gBufferStageParams;
		gBufferStageParams.SetClearTarget(gr::PipelineState::ClearTarget::ColorDepth);

		gBufferStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		gBufferStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Less);

		m_gBufferStage->SetStagePipelineState(gBufferStageParams);

		// Finally, append the render stage to the pipeline:
		pipeline.AppendRenderStage(m_gBufferStage);
	}


	void GBufferGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();

		// TODO: Support transparency
		// -> Mark meshes with transparent materials with a filter bit during load
		// -> Render in a separate forward pass
	}


	void GBufferGraphicsSystem::CreateBatches()
	{
		m_gBufferStage->AddBatches(RenderManager::Get()->GetSceneBatches());
	}


	std::shared_ptr<re::TextureTargetSet const> GBufferGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_gBufferStage->GetTextureTargetSet();
	}
}