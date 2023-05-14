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
		ENUM_TO_STR(GBufferWPos),		// 4
		ENUM_TO_STR(GBufferMatProp0),	// 5
		ENUM_TO_STR(GBufferDepth),		// 6
	};
	// TODO: Split this into 2 lists: color target names, and depth names
	// -> Often need to loop over color, and treat depth differently
	

	GBufferGraphicsSystem::GBufferGraphicsSystem(std::string name)
		: NamedObject(name)
		, GraphicsSystem(name)
		, m_gBufferStage("GBuffer Stage")		
	{
	}


	void GBufferGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		// Shader:
		std::shared_ptr<Shader> gBufferShader = 
			re::Shader::Create(Config::Get()->GetValue<string>("gBufferFillShaderName"));

		m_gBufferStage.SetStageShader(gBufferShader);

		// Create GBuffer color targets:
		Texture::TextureParams gBufferTexParams;
		gBufferTexParams.m_width = Config::Get()->GetValue<int>(en::Config::k_windowXResValueName);
		gBufferTexParams.m_height = Config::Get()->GetValue<int>(en::Config::k_windowYResValueName);
		gBufferTexParams.m_faces = 1;
		gBufferTexParams.m_usage = re::Texture::Usage::ColorTarget;
		gBufferTexParams.m_dimension = re::Texture::Dimension::Texture2D;
		gBufferTexParams.m_format = re::Texture::Format::RGBA32F; // Using 4 channels for future flexibility
		gBufferTexParams.m_colorSpace = re::Texture::ColorSpace::sRGB;
		gBufferTexParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		gBufferTexParams.m_addToSceneData = false;

		gBufferTexParams.m_useMIPs = false;
		// TODO: Currently, our GBuffer doesn't use mipmapping, but it should.
		// We need to compute the appropriate mip level in the shader, by writing UV derivatives during the GBuffer
		// pass, and using a stencil mask to ensure we're sampling the correct material at boundaries
		// https://www.reedbeta.com/blog/deferred-texturing/
		// -> We'll also need to trigger mip generation after laying down the GBuffer


		std::shared_ptr<re::TextureTargetSet> gBufferTargets = m_gBufferStage.GetTextureTargetSet();
		for (uint8_t i = GBufferTexIdx::GBufferAlbedo; i <= GBufferTexIdx::GBufferMatProp0; i++)
		{
			gBufferTargets->SetColorTarget(
				i, re::Texture::Create(GBufferTexNames[i], gBufferTexParams, false));
		}

		// Create GBuffer depth target:
		re::Texture::TextureParams depthTexParams(gBufferTexParams);
		depthTexParams.m_usage = re::Texture::Usage::DepthTarget;
		depthTexParams.m_format = re::Texture::Format::Depth32F;
		depthTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
				
		gBufferTargets->SetDepthStencilTarget(
			re::Texture::Create(GBufferTexNames[GBufferTexIdx::GBufferDepth], depthTexParams, false));

		// Camera:
		m_gBufferStage.AddPermanentParameterBlock(SceneManager::GetSceneData()->GetMainCamera()->GetCameraParams());

		// Set the stage params:
		gr::PipelineState gBufferStageParams;
		gBufferStageParams.SetClearTarget(gr::PipelineState::ClearTarget::ColorDepth);

		gBufferStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		gBufferStageParams.SetSrcBlendMode(gr::PipelineState::BlendMode::Disabled);
		gBufferStageParams.SetDstBlendMode(gr::PipelineState::BlendMode::Disabled);
		gBufferStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Less);

		m_gBufferStage.SetStagePipelineState(gBufferStageParams);

		// Finally, append the render stage to the pipeline:
		pipeline.AppendRenderStage(&m_gBufferStage);
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
		m_gBufferStage.AddBatches(RenderManager::Get()->GetSceneBatches());
	}


	std::shared_ptr<re::TextureTargetSet> GBufferGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_gBufferStage.GetTextureTargetSet();
	}
}