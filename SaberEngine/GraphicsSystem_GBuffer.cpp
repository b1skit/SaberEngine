#include <memory>

#include "GraphicsSystem_GBuffer.h"
#include "CoreEngine.h"
#include "Shader.h"
#include "Texture.h"
#include "Scene.h"

using gr::Shader;
using gr::Texture;

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
		m_gBufferStage("GBuffer Stage")
	{
	}


	void GBufferGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		// Shader:
		std::shared_ptr<Shader> gBufferShader = std::make_shared<Shader>(
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("gBufferFillShaderName"));
		gBufferShader->Create();

		// Shader constants: Only set once here
		const float emissiveIntensity =
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<float>("defaultSceneEmissiveIntensity");
		gBufferShader->SetUniform("emissiveIntensity", &emissiveIntensity, platform::Shader::UniformType::Float, 1);

		// Set the shader:
		m_gBufferStage.GetStageShader() = gBufferShader;

		// Create GBuffer color targets:
		Texture::TextureParams gBufferParams;
		gBufferParams.m_width = en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes");
		gBufferParams.m_height = en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes");
		gBufferParams.m_faces = 1;
		gBufferParams.m_texUse = gr::Texture::TextureUse::ColorTarget;
		gBufferParams.m_texDimension = gr::Texture::TextureDimension::Texture2D;
		gBufferParams.m_texFormat = gr::Texture::TextureFormat::RGBA32F; // Using 4 channels for future flexibility
		gBufferParams.m_texColorSpace = gr::Texture::TextureColorSpace::sRGB;
		gBufferParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);


		gBufferParams.m_useMIPs = false;
		// TODO: Currently, our GBuffer doesn't use mipmapping, but it should.
		// We need to compute the appropriate mip level in the shader, by writing UV derivatives during the GBuffer
		// pass, and using a stencil mask to ensure we're sampling the correct material at boundaries
		// https://www.reedbeta.com/blog/deferred-texturing/
		// -> We'll also need to trigger mip generation after laying down the GBuffer


		gr::TextureTargetSet& gBufferTargets = m_gBufferStage.GetTextureTargetSet();
		for (size_t i = 0; i <= 5; i++)
		{
			std::shared_ptr<gr::Texture> gBufferTex = std::make_shared<gr::Texture>(gBufferParams);

			gBufferTex->SetTexturePath(GBufferTexNames[i]);

			gBufferTargets.ColorTarget(i) = gBufferTex;
		}

		// Create GBuffer depth target:
		gr::Texture::TextureParams depthTexParams(gBufferParams);
		depthTexParams.m_texUse = gr::Texture::TextureUse::DepthTarget;
		depthTexParams.m_texFormat = gr::Texture::TextureFormat::Depth32F;
		depthTexParams.m_texColorSpace = gr::Texture::TextureColorSpace::Linear;

		std::shared_ptr<gr::Texture> depthTex = std::make_shared<gr::Texture>(depthTexParams);

		depthTex->SetTexturePath(GBufferTexNames[Material::GBufferDepth]); //TODO: Better indexing system

		gBufferTargets.DepthStencilTarget() = depthTex;

		// Initialize the target set:
		gBufferTargets.CreateColorDepthStencilTargets();

		// Camera:
		m_gBufferStage.GetStageCamera() =
			en::CoreEngine::GetSceneManager()->GetCameras(SaberEngine::CAMERA_TYPE_MAIN).at(0);

		// Set the stage params:
		RenderStage::RenderStageParams gBufferStageParams;
		gBufferStageParams.m_targetClearMode = platform::Context::ClearTarget::ColorDepth;

		gBufferStageParams.m_faceCullingMode	= platform::Context::FaceCullingMode::Back;
		gBufferStageParams.m_srcBlendMode		= platform::Context::BlendMode::Disabled;
		gBufferStageParams.m_dstBlendMode		= platform::Context::BlendMode::Disabled;
		gBufferStageParams.m_depthMode			= platform::Context::DepthMode::Less;
		gBufferStageParams.m_stageType			= RenderStage::RenderStageType::ColorAndDepth;

		m_gBufferStage.SetStageParams(gBufferStageParams);

		// Finally, append the render stage to the pipeline:
		pipeline.AppendRenderStage(m_gBufferStage);
	}


	void GBufferGraphicsSystem::PreRender()
	{
		m_gBufferStage.SetGeometryBatches(&en::CoreEngine::GetSceneManager()->GetRenderMeshes());

		// TODO: Support transparency
		// -> Split meshes with transparent materials out from opaque during load
		// -> Render in a separate forward mode
	}
}