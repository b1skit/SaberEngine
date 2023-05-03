// © 2022 Adam Badke. All rights reserved.
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "Shader.h"
#include "Config.h"
#include "SceneManager.h"
#include "RenderManager.h"


namespace gr
{
	using gr::DeferredLightingGraphicsSystem;
	using en::Config;
	using en::SceneManager;
	using re::RenderManager;
	using re::RenderStage;
	using re::Sampler;
	using re::Shader;
	using re::Batch;
	using re::Texture;
	using std::shared_ptr;
	using std::make_shared;
	using std::string;
	using std::to_string;
	using glm::vec3;
	using glm::vec4;


	BloomGraphicsSystem::BloomGraphicsSystem(std::string name) : GraphicsSystem(name), NamedObject(name),
		m_emissiveBlitStage("Emissive blit stage")
	{
		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Near);
	}


	void BloomGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		shared_ptr<DeferredLightingGraphicsSystem> deferredLightGS = dynamic_pointer_cast<DeferredLightingGraphicsSystem>(
			RenderManager::Get()->GetGraphicsSystem<DeferredLightingGraphicsSystem>());

		Camera* sceneCam = SceneManager::GetSceneData()->GetMainCamera().get();

		shared_ptr<Shader> blitShader = re::Shader::Create(Config::Get()->GetValue<string>("blitShaderName"));

		// Emissive blit stage:
		gr::PipelineState emissiveStageParams;
		emissiveStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
		emissiveStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		emissiveStageParams.SetSrcBlendMode(gr::PipelineState::BlendMode::One);
		emissiveStageParams.SetDstBlendMode(gr::PipelineState::BlendMode::One);
		emissiveStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Always);

		m_emissiveBlitStage.SetStagePipelineState(emissiveStageParams);
		m_emissiveBlitStage.SetStageShader(blitShader);
		m_emissiveBlitStage.AddPermanentParameterBlock(sceneCam->GetCameraParams());

		m_emissiveBlitStage.SetTextureTargetSet(deferredLightGS->GetFinalTextureTargetSet());
		
		pipeline.AppendRenderStage(&m_emissiveBlitStage);


		// Bloom stages:
		gr::PipelineState bloomStageParams;
		bloomStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
		bloomStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		bloomStageParams.SetSrcBlendMode(gr::PipelineState::BlendMode::One);
		bloomStageParams.SetDstBlendMode(gr::PipelineState::BlendMode::Zero);
		bloomStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Always);
		
		const uint32_t numScalingStages = m_numDownSamplePasses;
		m_downResStages.reserve(numScalingStages); // MUST reserve so our pointers won't change

		int currentXRes = Config::Get()->GetValue<int>(en::Config::k_windowXResValueName) / 2;
		int currentYRes = Config::Get()->GetValue<int>(en::Config::k_windowYResValueName) / 2;

		Texture::TextureParams resScaleParams;
		resScaleParams.m_width = currentXRes;
		resScaleParams.m_height = currentYRes;
		resScaleParams.m_faces = 1;
		resScaleParams.m_usage = Texture::Usage::ColorTarget;
		resScaleParams.m_dimension = Texture::Dimension::Texture2D;
		resScaleParams.m_format = Texture::Format::RGBA32F;
		resScaleParams.m_colorSpace = Texture::ColorSpace::Linear;
		resScaleParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		resScaleParams.m_useMIPs = false;

		shared_ptr<Shader> luminanceThresholdShader = re::Shader::Create(
			Config::Get()->GetValue<string>("blurShaderName"));
		luminanceThresholdShader->ShaderKeywords().emplace_back("BLUR_SHADER_LUMINANCE_THRESHOLD");

		// Downsampling stages (w/luminance threshold in 1st pass):
		for (uint32_t i = 0; i < numScalingStages; i++)
		{
			m_downResStages.emplace_back("Down-res stage " + to_string(i + 1) + " / " + to_string(numScalingStages));

			m_downResStages.back().GetTextureTargetSet()->Viewport().Width() = currentXRes;
			m_downResStages.back().GetTextureTargetSet()->Viewport().Height() = currentYRes;

			resScaleParams.m_width = currentXRes;
			resScaleParams.m_height = currentYRes;
			const string texPath = "ScaledResolution_" + to_string(currentXRes) + "x" + to_string(currentYRes);

			m_downResStages.back().GetTextureTargetSet()->SetColorTarget(0, 
				std::make_shared<re::Texture>(texPath, resScaleParams, false));

			m_downResStages.back().SetStagePipelineState(bloomStageParams);
			m_downResStages.back().AddPermanentParameterBlock(sceneCam->GetCameraParams());

			if (i == 0)
			{
				m_downResStages[i].SetStageShader(luminanceThresholdShader);
			}
			else
			{
				m_downResStages[i].SetStageShader(blitShader);
			}

			pipeline.AppendRenderStage(&m_downResStages[i]);

			// Don't halve the resolution on the last iteration:
			if (i < (numScalingStages - 1))
			{
				currentXRes /= 2;
				currentYRes /= 2;
			}
		}

		// Blur stages:
		shared_ptr<Shader> horizontalBlurShader = re::Shader::Create(Config::Get()->GetValue<string>("blurShaderName"));
		horizontalBlurShader->ShaderKeywords().emplace_back("BLUR_SHADER_HORIZONTAL");

		shared_ptr<Shader> verticalBlurShader = re::Shader::Create(Config::Get()->GetValue<string>("blurShaderName"));
		verticalBlurShader->ShaderKeywords().emplace_back("BLUR_SHADER_VERTICAL");

		Texture::TextureParams blurParams(resScaleParams);
		blurParams.m_width = currentXRes;
		blurParams.m_height = currentYRes;
		const string texName = "BlurPingPong_" + to_string(currentXRes) + "x" + to_string(currentYRes);
		
		shared_ptr<Texture> blurPingPongTexture = make_shared<re::Texture>(texName, blurParams, false);

		uint32_t totalBlurPasses = m_numBlurPasses * 2; // x2 for horizontal + blur separation
		m_blurStages.reserve(totalBlurPasses);  // MUST reserve so our pointers won't change
		for (size_t i = 0; i < totalBlurPasses; i++)
		{
			const string stagePrefix = (i % 2 == 0) ? "Horizontal " : "Vertical ";
			m_blurStages.emplace_back(
				stagePrefix + "blur stage " + to_string((i+2)/2) + " / " + to_string(m_numBlurPasses));

			m_blurStages.back().GetTextureTargetSet()->Viewport().Width() = currentXRes;
			m_blurStages.back().GetTextureTargetSet()->Viewport().Height() = currentYRes;

			m_blurStages.back().SetStagePipelineState(bloomStageParams);
			m_blurStages.back().AddPermanentParameterBlock(sceneCam->GetCameraParams());

			if (i % 2 == 0)
			{
				m_blurStages.back().GetTextureTargetSet()->SetColorTarget(0, blurPingPongTexture);
				m_blurStages.back().SetStageShader(horizontalBlurShader);
			}
			else
			{
				m_blurStages.back().GetTextureTargetSet()->SetColorTarget(0,
					m_downResStages.back().GetTextureTargetSet()->GetColorTarget(0));
				m_blurStages.back().SetStageShader(verticalBlurShader);
			}

			pipeline.AppendRenderStage(&m_blurStages[i]);
		}

		// Up-res stages:
		m_upResStages.reserve(numScalingStages); // MUST reserve so our pointers won't change
		for (size_t i = 0; i < numScalingStages; i++)
		{
			currentXRes *= 2;
			currentYRes *= 2;

			m_upResStages.emplace_back("Up-res stage " + to_string(i + 1) + " / " + to_string(numScalingStages));

			m_upResStages.back().GetTextureTargetSet()->Viewport().Width() = currentXRes;
			m_upResStages.back().GetTextureTargetSet()->Viewport().Height() = currentYRes;
	
			m_upResStages.back().AddPermanentParameterBlock(sceneCam->GetCameraParams());
			m_upResStages[i].SetStageShader(blitShader);

			if (i == (numScalingStages - 1)) // Last iteration: Additive blit back to the src gs
			{
				m_upResStages.back().SetTextureTargetSet(deferredLightGS->GetFinalTextureTargetSet());

				gr::PipelineState addStageParams(bloomStageParams);
				addStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
				addStageParams.SetSrcBlendMode(gr::PipelineState::BlendMode::One);
				addStageParams.SetDstBlendMode(gr::PipelineState::BlendMode::One);

				m_upResStages.back().SetStagePipelineState(addStageParams);
			}
			else
			{
				m_upResStages.back().GetTextureTargetSet()->SetColorTarget(0,
					m_downResStages[m_downResStages.size() - (i + 2)].GetTextureTargetSet()->GetColorTarget(0));

				m_upResStages.back().SetStagePipelineState(bloomStageParams);
			}

			pipeline.AppendRenderStage(&m_upResStages[i]);

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
		CreateBatches();

		shared_ptr<GBufferGraphicsSystem> gbufferGS = dynamic_pointer_cast<GBufferGraphicsSystem>(
			RenderManager::Get()->GetGraphicsSystem<GBufferGraphicsSystem>());

		shared_ptr<DeferredLightingGraphicsSystem> deferredLightGS =
			dynamic_pointer_cast<DeferredLightingGraphicsSystem>(
				RenderManager::Get()->GetGraphicsSystem<DeferredLightingGraphicsSystem>());

		shared_ptr<Sampler> const bloomStageSampler = Sampler::GetSampler(Sampler::WrapAndFilterMode::ClampLinearLinear);

		// This index corresponds with the GBuffer texture layout bindings in SaberCommon.glsl
		// TODO: Have a less brittle way of handling this.
		const size_t gBufferEmissiveTextureIndex = 3; 
		m_emissiveBlitStage.SetPerFrameTextureInput(
			"GBufferAlbedo",
			gbufferGS->GetFinalTextureTargetSet()->GetColorTarget(gBufferEmissiveTextureIndex).GetTexture(),
			bloomStageSampler);

		for (size_t i = 0; i < m_downResStages.size(); i++)
		{
			if (i == 0)
			{
				m_downResStages[i].SetPerFrameTextureInput(
					"GBufferAlbedo", 
					m_emissiveBlitStage.GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_downResStages[i].SetPerFrameTextureInput(
					"GBufferAlbedo",
					m_downResStages[i - 1].GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
		}

		for (size_t i = 0; i < m_blurStages.size(); i++)
		{
			if (i == 0)
			{
				m_blurStages[i].SetPerFrameTextureInput(
					"GBufferAlbedo",
					m_downResStages.back().GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_blurStages[i].SetPerFrameTextureInput(
					"GBufferAlbedo",
					m_blurStages[i-1].GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
		}

		for (size_t i = 0; i < m_upResStages.size(); i++)
		{
			if (i == 0)
			{
				m_upResStages[i].SetPerFrameTextureInput(
					"GBufferAlbedo",
					m_blurStages.back().GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_upResStages[i].SetPerFrameTextureInput(
					"GBufferAlbedo",
					m_upResStages[i-1].GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
		}
	}


	void BloomGraphicsSystem::CreateBatches()
	{
		const Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);

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