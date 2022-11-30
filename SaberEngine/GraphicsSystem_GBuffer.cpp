#include <memory>

#include "GraphicsSystem_GBuffer.h"
#include "Config.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "Shader.h"
#include "Texture.h"
#include "SceneData.h"

using gr::Shader;
using gr::Texture;
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


	GBufferGraphicsSystem::GBufferGraphicsSystem(std::string name) : GraphicsSystem(name), 
		m_gBufferStage("GBuffer Stage"), NamedObject(name)
	{
	}


	void GBufferGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		// Shader:
		std::shared_ptr<Shader> gBufferShader = std::make_shared<Shader>(
			Config::Get()->GetValue<string>("gBufferFillShaderName"));
		gBufferShader->Create();

		// Shader constants: Only set once here
		const float emissiveIntensity =
			Config::Get()->GetValue<float>("defaultSceneEmissiveIntensity");
		gBufferShader->SetUniform("emissiveIntensity", &emissiveIntensity, platform::Shader::UniformType::Float, 1);

		// Set the shader:
		m_gBufferStage.GetStageShader() = gBufferShader;

		// Create GBuffer color targets:
		Texture::TextureParams gBufferParams;
		gBufferParams.m_width = Config::Get()->GetValue<int>("windowXRes");
		gBufferParams.m_height = Config::Get()->GetValue<int>("windowYRes");
		gBufferParams.m_faces = 1;
		gBufferParams.m_usage = gr::Texture::Usage::ColorTarget;
		gBufferParams.m_dimension = gr::Texture::Dimension::Texture2D;
		gBufferParams.m_format = gr::Texture::Format::RGBA32F; // Using 4 channels for future flexibility
		gBufferParams.m_colorSpace = gr::Texture::ColorSpace::sRGB;
		gBufferParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);


		gBufferParams.m_useMIPs = false;
		// TODO: Currently, our GBuffer doesn't use mipmapping, but it should.
		// We need to compute the appropriate mip level in the shader, by writing UV derivatives during the GBuffer
		// pass, and using a stencil mask to ensure we're sampling the correct material at boundaries
		// https://www.reedbeta.com/blog/deferred-texturing/
		// -> We'll also need to trigger mip generation after laying down the GBuffer


		re::TextureTargetSet& gBufferTargets = m_gBufferStage.GetTextureTargetSet();
		for (size_t i = 0; i <= 5; i++)
		{
			std::shared_ptr<gr::Texture> gBufferTex = std::make_shared<gr::Texture>(GBufferTexNames[i], gBufferParams);
			gBufferTargets.ColorTarget(i) = gBufferTex;
		}

		// Create GBuffer depth target:
		gr::Texture::TextureParams depthTexParams(gBufferParams);
		depthTexParams.m_usage = gr::Texture::Usage::DepthTarget;
		depthTexParams.m_format = gr::Texture::Format::Depth32F;
		depthTexParams.m_colorSpace = gr::Texture::ColorSpace::Linear;

		const size_t gBufferDepthTextureNameIdx = 6; //TODO: Handle this in a less brittle way

		std::shared_ptr<gr::Texture> depthTex = 
			std::make_shared<gr::Texture>(GBufferTexNames[gBufferDepthTextureNameIdx], depthTexParams);
		

		gBufferTargets.DepthStencilTarget() = depthTex;

		// Initialize the target set:
		gBufferTargets.CreateColorDepthStencilTargets();

		// Camera:
		m_gBufferStage.GetStageCamera() = SceneManager::GetSceneData()->GetMainCamera().get();

		// Set the stage params:
		RenderStage::PipelineStateParams gBufferStageParams;
		gBufferStageParams.m_targetClearMode = platform::Context::ClearTarget::ColorDepth;

		gBufferStageParams.m_faceCullingMode	= platform::Context::FaceCullingMode::Back;
		gBufferStageParams.m_srcBlendMode		= platform::Context::BlendMode::Disabled;
		gBufferStageParams.m_dstBlendMode		= platform::Context::BlendMode::Disabled;
		gBufferStageParams.m_depthTestMode		= platform::Context::DepthTestMode::Less;

		m_gBufferStage.SetStagePipelineStateParams(gBufferStageParams);

		// Finally, append the render stage to the pipeline:
		pipeline.AppendRenderStage(m_gBufferStage);
	}


	void GBufferGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		m_gBufferStage.InitializeForNewFrame();

		CreateBatches();

		// TODO: Support transparency
		// -> Mark meshes with transparent materials with a filter bit during load
		// -> Render in a separate forward mode
	}


	void GBufferGraphicsSystem::CreateBatches()
	{
		m_gBufferStage.AddBatches(RenderManager::Get()->GetSceneBatches());
	}
}