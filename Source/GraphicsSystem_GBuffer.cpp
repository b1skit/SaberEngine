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
	// These names are ordered to align with the layout binding indexes defined in SaberCommon.glsl
	const std::vector<std::string> GBufferGraphicsSystem::GBufferTexNames
	{
		"GBufferAlbedo",	// 0
		"GBufferWNormal",	// 1
		"GBufferRMAO",		// 2
		"GBufferEmissive",	// 3
		"GBufferWPos",		// 4
		"GBufferMatProp0",	// 5
		"GBufferDepth",		// 6
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
			std::make_shared<Shader>(Config::Get()->GetValue<string>("gBufferFillShaderName"));

		m_gBufferStage.GetStageShader() = gBufferShader;

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


		gBufferTexParams.m_useMIPs = false;
		// TODO: Currently, our GBuffer doesn't use mipmapping, but it should.
		// We need to compute the appropriate mip level in the shader, by writing UV derivatives during the GBuffer
		// pass, and using a stencil mask to ensure we're sampling the correct material at boundaries
		// https://www.reedbeta.com/blog/deferred-texturing/
		// -> We'll also need to trigger mip generation after laying down the GBuffer


		std::shared_ptr<re::TextureTargetSet> gBufferTargets = m_gBufferStage.GetTextureTargetSet();
		for (size_t i = 0; i <= 5; i++)
		{
			gBufferTargets->SetColorTarget(i, std::make_shared<re::Texture>(GBufferTexNames[i], gBufferTexParams));
		}

		// Create GBuffer depth target:
		re::Texture::TextureParams depthTexParams(gBufferTexParams);
		depthTexParams.m_usage = re::Texture::Usage::DepthTarget;
		depthTexParams.m_format = re::Texture::Format::Depth32F;
		depthTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;

		const size_t gBufferDepthTextureNameIdx = 6; //TODO: Handle this in a less brittle way
				
		gBufferTargets->SetDepthStencilTarget(
			std::make_shared<re::Texture>(GBufferTexNames[gBufferDepthTextureNameIdx], depthTexParams));

		// Camera:
		m_gBufferStage.SetStageCamera(SceneManager::GetSceneData()->GetMainCamera().get());

		// Set the stage params:
		gr::PipelineState gBufferStageParams;
		gBufferStageParams.m_targetClearMode = gr::PipelineState::ClearTarget::ColorDepth;

		gBufferStageParams.m_faceCullingMode	= gr::PipelineState::FaceCullingMode::Back;
		gBufferStageParams.m_srcBlendMode		= gr::PipelineState::BlendMode::Disabled;
		gBufferStageParams.m_dstBlendMode		= gr::PipelineState::BlendMode::Disabled;
		gBufferStageParams.m_depthTestMode		= gr::PipelineState::DepthTestMode::Less;

		m_gBufferStage.SetStagePipelineState(gBufferStageParams);

		// Finally, append the render stage to the pipeline:
		pipeline.AppendRenderStage(m_gBufferStage);
	}


	void GBufferGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();

		// TODO: Support transparency
		// -> Mark meshes with transparent materials with a filter bit during load
		// -> Render in a separate forward mode
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