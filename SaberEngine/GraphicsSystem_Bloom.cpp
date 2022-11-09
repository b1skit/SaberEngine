#include <memory>
#include <string>

#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "Shader.h"
#include "CoreEngine.h"

using gr::DeferredLightingGraphicsSystem;
using re::Batch;
using en::CoreEngine;
using std::shared_ptr;
using std::make_shared;
using std::string;
using std::to_string;
using glm::vec3;
using glm::vec4;


namespace gr
{
	BloomGraphicsSystem::BloomGraphicsSystem(std::string name) : GraphicsSystem(name), NamedObject(name),
		m_emissiveBlitStage("Emissive blit stage")
	{
		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Near);
	}


	void BloomGraphicsSystem::Create(re::StagePipeline& pipeline)
	{

		shared_ptr<DeferredLightingGraphicsSystem> deferredLightGS = dynamic_pointer_cast<DeferredLightingGraphicsSystem>(
			CoreEngine::GetRenderManager()->GetGraphicsSystem<DeferredLightingGraphicsSystem>());

		shared_ptr<Camera> sceneCam = CoreEngine::GetSceneManager()->GetSceneData()->GetMainCamera();

		shared_ptr<Shader> blitShader = make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blitShaderName"));
		blitShader->Create();

		// Emissive blit stage:
		RenderStage::RenderStageParams emissiveStageParams;
		emissiveStageParams.m_targetClearMode	= platform::Context::ClearTarget::None;
		emissiveStageParams.m_faceCullingMode	= platform::Context::FaceCullingMode::Back;
		emissiveStageParams.m_srcBlendMode		= platform::Context::BlendMode::One;
		emissiveStageParams.m_dstBlendMode		= platform::Context::BlendMode::One;
		emissiveStageParams.m_depthTestMode		= platform::Context::DepthTestMode::Always;

		m_emissiveBlitStage.SetRenderStageParams(emissiveStageParams);
		m_emissiveBlitStage.GetStageShader() = blitShader;
		m_emissiveBlitStage.GetStageCamera() = sceneCam;

		m_emissiveBlitStage.GetTextureTargetSet() = deferredLightGS->GetFinalTextureTargetSet();
		
		pipeline.AppendRenderStage(m_emissiveBlitStage);


		// Bloom stages:
		RenderStage::RenderStageParams bloomStageParams;
		bloomStageParams.m_targetClearMode	= platform::Context::ClearTarget::None;
		bloomStageParams.m_faceCullingMode	= platform::Context::FaceCullingMode::Back;
		bloomStageParams.m_srcBlendMode		= platform::Context::BlendMode::One;
		bloomStageParams.m_dstBlendMode		= platform::Context::BlendMode::Zero;
		bloomStageParams.m_depthTestMode	= platform::Context::DepthTestMode::Always;
		
		const uint32_t numScalingStages = m_numDownSamplePasses;
		m_downResStages.reserve(numScalingStages); // MUST reserve so our pointers won't change

		int currentXRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes") / 2;
		int currentYRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes") / 2;

		Texture::TextureParams resScaleParams;
		resScaleParams.m_width = currentXRes;
		resScaleParams.m_height = currentYRes;
		resScaleParams.m_faces = 1;
		resScaleParams.m_texUse = Texture::TextureUse::ColorTarget;
		resScaleParams.m_texDimension = Texture::TextureDimension::Texture2D;
		resScaleParams.m_texFormat = Texture::TextureFormat::RGBA32F;
		resScaleParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
		resScaleParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		resScaleParams.m_useMIPs = false;

		shared_ptr<Shader> luminanceThresholdShader = make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShaderName"));
		luminanceThresholdShader->ShaderKeywords().emplace_back("BLUR_SHADER_LUMINANCE_THRESHOLD");
		luminanceThresholdShader->Create();

		// Downsampling stages (w/luminance threshold in 1st pass):
		for (uint32_t i = 0; i < numScalingStages; i++)
		{
			m_downResStages.emplace_back("Down-res stage " + to_string(i + 1) + " / " + to_string(numScalingStages));

			m_downResStages.back().GetTextureTargetSet().Viewport().Width() = currentXRes;
			m_downResStages.back().GetTextureTargetSet().Viewport().Height() = currentYRes;

			resScaleParams.m_width = currentXRes;
			resScaleParams.m_height = currentYRes;
			const string texPath = "ScaledResolution_" + to_string(currentXRes) + "x" + to_string(currentYRes);

			m_downResStages.back().GetTextureTargetSet().ColorTarget(0) = 
				std::make_shared<gr::Texture>(texPath, resScaleParams);
			m_downResStages.back().GetTextureTargetSet().CreateColorTargets();

			m_downResStages.back().SetRenderStageParams(bloomStageParams);
			m_downResStages.back().GetStageCamera() = sceneCam;

			if (i == 0)
			{
				m_downResStages[i].GetStageShader() = luminanceThresholdShader;
			}
			else
			{
				m_downResStages[i].GetStageShader() = blitShader;
			}

			pipeline.AppendRenderStage(m_downResStages[i]);

			// Don't halve the resolution on the last iteration:
			if (i < (numScalingStages - 1))
			{
				currentXRes /= 2;
				currentYRes /= 2;
			}
		}

		// Blur stages:
		shared_ptr<Shader> horizontalBlurShader = make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShaderName"));
		horizontalBlurShader->ShaderKeywords().emplace_back("BLUR_SHADER_HORIZONTAL");
		horizontalBlurShader->Create();

		shared_ptr<Shader> verticalBlurShader = make_shared<Shader>(
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("blurShaderName"));
		verticalBlurShader->ShaderKeywords().emplace_back("BLUR_SHADER_VERTICAL");
		verticalBlurShader->Create();

		Texture::TextureParams blurParams(resScaleParams);
		blurParams.m_width = currentXRes;
		blurParams.m_height = currentYRes;
		const string texName = "BlurPingPong_" + to_string(currentXRes) + "x" + to_string(currentYRes);
		
		shared_ptr<Texture> blurPingPongTexture = make_shared<gr::Texture>(texName, blurParams);

		uint32_t totalBlurPasses = m_numBlurPasses * 2; // x2 for horizontal + blur separation
		m_blurStages.reserve(totalBlurPasses);  // MUST reserve so our pointers won't change
		for (size_t i = 0; i < totalBlurPasses; i++)
		{
			const string stagePrefix = (i % 2 == 0) ? "Horizontal " : "Vertical ";
			m_blurStages.emplace_back(
				stagePrefix + "blur stage " + to_string((i+2)/2) + " / " + to_string(m_numBlurPasses));

			m_blurStages.back().GetTextureTargetSet().Viewport().Width() = currentXRes;
			m_blurStages.back().GetTextureTargetSet().Viewport().Height() = currentYRes;

			m_blurStages.back().SetRenderStageParams(bloomStageParams);
			m_blurStages.back().GetStageCamera() = sceneCam;

			if (i % 2 == 0)
			{
				m_blurStages.back().GetTextureTargetSet().ColorTarget(0) = blurPingPongTexture;
				m_blurStages.back().GetTextureTargetSet().CreateColorTargets();
				m_blurStages.back().GetStageShader() = horizontalBlurShader;
			}
			else
			{
				m_blurStages.back().GetTextureTargetSet().ColorTarget(0) 
					= m_downResStages.back().GetTextureTargetSet().ColorTarget(0);
				m_blurStages.back().GetTextureTargetSet().CreateColorTargets();
				m_blurStages.back().GetStageShader() = verticalBlurShader;
			}

			pipeline.AppendRenderStage(m_blurStages[i]);
		}

		// Up-res stages:
		m_upResStages.reserve(numScalingStages); // MUST reserve so our pointers won't change
		for (size_t i = 0; i < numScalingStages; i++)
		{
			currentXRes *= 2;
			currentYRes *= 2;

			m_upResStages.emplace_back("Up-res stage " + to_string(i + 1) + " / " + to_string(numScalingStages));

			m_upResStages.back().GetTextureTargetSet().Viewport().Width() = currentXRes;
			m_upResStages.back().GetTextureTargetSet().Viewport().Height() = currentYRes;
	
			m_upResStages.back().GetStageCamera() = sceneCam;
			m_upResStages[i].GetStageShader() = blitShader;

			if (i == (numScalingStages - 1)) // Last iteration: Additive blit back to the src gs
			{
				m_upResStages.back().GetTextureTargetSet() = deferredLightGS->GetFinalTextureTargetSet();

				RenderStage::RenderStageParams addStageParams(bloomStageParams);
				addStageParams.m_targetClearMode	= platform::Context::ClearTarget::None;
				addStageParams.m_srcBlendMode		= platform::Context::BlendMode::One;
				addStageParams.m_dstBlendMode		= platform::Context::BlendMode::One;

				m_upResStages.back().SetRenderStageParams(addStageParams);
			}
			else
			{
				m_upResStages.back().GetTextureTargetSet().ColorTarget(0) = 
					m_downResStages[m_downResStages.size() - (i + 2)].GetTextureTargetSet().ColorTarget(0);
				m_upResStages.back().GetTextureTargetSet().CreateColorTargets();

				m_upResStages.back().SetRenderStageParams(bloomStageParams);
			}

			pipeline.AppendRenderStage(m_upResStages[i]);

			// Don't halve the resolution on the last iteration:
			if (i < (numScalingStages - 1))
			{
				currentXRes *= 2;
				currentYRes *= 2;
			}
		}
	}


	void BloomGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		m_emissiveBlitStage.InitializeForNewFrame();

		for (size_t i = 0; i < m_downResStages.size(); i++)
		{
			m_downResStages[i].InitializeForNewFrame();
		}
		for (size_t i = 0; i < m_blurStages.size(); i++)
		{
			m_blurStages[i].InitializeForNewFrame();
		}
		for (size_t i = 0; i < m_upResStages.size(); i++)
		{
			m_upResStages[i].InitializeForNewFrame();
		}

		CreateBatches();


		shared_ptr<GBufferGraphicsSystem> gbufferGS = dynamic_pointer_cast<GBufferGraphicsSystem>(
			CoreEngine::GetRenderManager()->GetGraphicsSystem<GBufferGraphicsSystem>());

		shared_ptr<DeferredLightingGraphicsSystem> deferredLightGS =
			dynamic_pointer_cast<DeferredLightingGraphicsSystem>(
				CoreEngine::GetRenderManager()->GetGraphicsSystem<DeferredLightingGraphicsSystem>());

		shared_ptr<Sampler const> const bloomStageSampler = Sampler::GetSampler(Sampler::SamplerType::ClampLinearLinear);

		// This index corresponds with the GBuffer texture layout bindings in SaberCommon.glsl
		// TODO: Have a less brittle way of handling this.
		const size_t gBufferEmissiveTextureIndex = 3; 
		m_emissiveBlitStage.SetTextureInput(
			"GBufferAlbedo",
			gbufferGS->GetFinalTextureTargetSet().ColorTarget(gBufferEmissiveTextureIndex).GetTexture(),
			bloomStageSampler);

		for (size_t i = 0; i < m_downResStages.size(); i++)
		{
			if (i == 0)
			{
				m_downResStages[i].SetTextureInput(
					"GBufferAlbedo", 
					m_emissiveBlitStage.GetTextureTargetSet().ColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_downResStages[i].SetTextureInput(
					"GBufferAlbedo",
					m_downResStages[i - 1].GetTextureTargetSet().ColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
		}

		for (size_t i = 0; i < m_blurStages.size(); i++)
		{
			if (i == 0)
			{
				m_blurStages[i].SetTextureInput(
					"GBufferAlbedo",
					m_downResStages.back().GetTextureTargetSet().ColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_blurStages[i].SetTextureInput(
					"GBufferAlbedo",
					m_blurStages[i-1].GetTextureTargetSet().ColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
		}

		for (size_t i = 0; i < m_upResStages.size(); i++)
		{
			if (i == 0)
			{
				m_upResStages[i].SetTextureInput(
					"GBufferAlbedo",
					m_blurStages.back().GetTextureTargetSet().ColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_upResStages[i].SetTextureInput(
					"GBufferAlbedo",
					m_upResStages[i-1].GetTextureTargetSet().ColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
		}
	}


	void BloomGraphicsSystem::CreateBatches()
	{
		const Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr, nullptr);

		m_emissiveBlitStage.AddBatch(fullscreenQuadBatch);

		for (size_t i = 0; i < m_downResStages.size(); i++)
		{
			m_downResStages[i].AddBatch(fullscreenQuadBatch);
		}
		for (size_t i = 0; i < m_blurStages.size(); i++)
		{
			m_blurStages[i].AddBatch(fullscreenQuadBatch);
		}
		for (size_t i = 0; i < m_upResStages.size(); i++)
		{
			m_upResStages[i].AddBatch(fullscreenQuadBatch);
		}
	}
}