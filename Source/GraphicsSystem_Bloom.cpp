// © 2022 Adam Badke. All rights reserved.
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "Shader.h"
#include "Config.h"
#include "SceneManager.h"
#include "RenderManager.h"

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


namespace
{
	struct BloomParams
	{
		glm::vec4 g_bloomTargetResolution;

		static constexpr char const* const s_shaderName = "BloomParams";
	};


	BloomParams CreateBloomParamsData(std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		BloomParams bloomParams;
		bloomParams.g_bloomTargetResolution = targetSet->GetTargetDimensions();
		return bloomParams;
	}
}

namespace gr
{
	BloomGraphicsSystem::BloomGraphicsSystem(std::string name)
		: GraphicsSystem(name)
		, NamedObject(name)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_emissiveBlitStage = re::RenderStage::CreateGraphicsStage("Emissive blit stage", gfxStageParams);

		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Near);
	}


	void BloomGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		DeferredLightingGraphicsSystem* deferredLightGS = 
			RenderManager::Get()->GetGraphicsSystem<DeferredLightingGraphicsSystem>();

		Camera* sceneCam = SceneManager::GetSceneData()->GetMainCamera().get();

		shared_ptr<Shader> blitShader = re::Shader::Create(Config::Get()->GetValue<string>("blitShaderName"));

		// Emissive blit stage:
		gr::PipelineState emissiveStageParams;
		emissiveStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
		emissiveStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		emissiveStageParams.SetSrcBlendMode(gr::PipelineState::BlendMode::One);
		emissiveStageParams.SetDstBlendMode(gr::PipelineState::BlendMode::One);
		emissiveStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Always);

		m_emissiveBlitStage->SetStagePipelineState(emissiveStageParams);
		m_emissiveBlitStage->SetStageShader(blitShader);
		m_emissiveBlitStage->AddPermanentParameterBlock(sceneCam->GetCameraParams());

		m_emissiveBlitStage->SetTextureTargetSet(
			re::TextureTargetSet::Create(*deferredLightGS->GetFinalTextureTargetSet(), "Emissive Blit Target Set"));
		
		pipeline.AppendRenderStage(m_emissiveBlitStage);

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
		resScaleParams.m_useMIPs = false;
		resScaleParams.m_addToSceneData = false;

		re::TextureTarget::TargetParams targetParams;
		targetParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		shared_ptr<Shader> luminanceThresholdShader =
			re::Shader::Create(Config::Get()->GetValue<string>("luminanceThresholdShaderName"));

		// Downsampling stages (w/luminance threshold in 1st pass):
		for (uint32_t i = 0; i < numScalingStages; i++)
		{
			const string name = "Down-res stage " + to_string(i + 1) + " / " + to_string(numScalingStages);
			re::RenderStage::GraphicsStageParams gfxStageParams;
			m_downResStages.emplace_back(re::RenderStage::CreateGraphicsStage(name, gfxStageParams));

			std::shared_ptr<re::TextureTargetSet> downResTargets = re::TextureTargetSet::Create(name + " targets");

			downResTargets->Viewport().Width() = currentXRes;
			downResTargets->Viewport().Height() = currentYRes;

			resScaleParams.m_width = currentXRes;
			resScaleParams.m_height = currentYRes;
			const string texPath = "ScaledResolution_" + to_string(currentXRes) + "x" + to_string(currentYRes);

			downResTargets->SetColorTarget(0, re::Texture::Create(texPath, resScaleParams, false), targetParams);

			m_downResStages.back()->SetTextureTargetSet(downResTargets);

			m_downResStages.back()->SetStagePipelineState(bloomStageParams);
			m_downResStages.back()->AddPermanentParameterBlock(sceneCam->GetCameraParams());

			if (i == 0)
			{
				m_downResStages[i]->SetStageShader(luminanceThresholdShader);
			}
			else
			{
				m_downResStages[i]->SetStageShader(blitShader);
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
		shared_ptr<Shader> horizontalBlurShader = 
			re::Shader::Create(Config::Get()->GetValue<string>("blurShaderHorizontalShaderName"));

		shared_ptr<Shader> verticalBlurShader = 
			re::Shader::Create(Config::Get()->GetValue<string>("blurShaderVerticalShaderName"));

		Texture::TextureParams blurParams(resScaleParams);
		blurParams.m_width = currentXRes;
		blurParams.m_height = currentYRes;
		const string texName = "BlurPingPong_" + to_string(currentXRes) + "x" + to_string(currentYRes);
		
		shared_ptr<Texture> blurPingPongTexture = re::Texture::Create(texName, blurParams, false);

		uint32_t totalBlurPasses = m_numBlurPasses * 2; // x2 for horizontal + blur separation
		m_blurStages.reserve(totalBlurPasses);  // MUST reserve so our pointers won't change
		for (size_t i = 0; i < totalBlurPasses; i++)
		{
			const string stagePrefix = (i % 2 == 0) ? "Horizontal " : "Vertical ";
			const string name = stagePrefix + "blur stage " + to_string((i + 2) / 2) + " / " + to_string(m_numBlurPasses);
			re::RenderStage::GraphicsStageParams gfxStageParams;
			m_blurStages.emplace_back(re::RenderStage::CreateGraphicsStage(name, gfxStageParams));

			std::shared_ptr<re::TextureTargetSet> blurTargets = re::TextureTargetSet::Create(name + " targets");

			blurTargets->Viewport().Width() = currentXRes;
			blurTargets->Viewport().Height() = currentYRes;

			m_blurStages.back()->SetStagePipelineState(bloomStageParams);
			m_blurStages.back()->AddPermanentParameterBlock(sceneCam->GetCameraParams());

			if (i % 2 == 0)
			{
				blurTargets->SetColorTarget(0, blurPingPongTexture, targetParams);
				m_blurStages.back()->SetStageShader(horizontalBlurShader);
			}
			else
			{
				blurTargets->SetColorTarget(0, m_downResStages.back()->GetTextureTargetSet()->GetColorTarget(0));
				m_blurStages.back()->SetStageShader(verticalBlurShader);
			}
			m_blurStages.back()->SetTextureTargetSet(blurTargets);

			m_blurStages.back()->AddPermanentParameterBlock(re::ParameterBlock::Create(
				BloomParams::s_shaderName,
				CreateBloomParamsData(blurTargets),
				re::ParameterBlock::PBType::Immutable));

			pipeline.AppendRenderStage(m_blurStages[i]);
		}

		// Up-res stages:
		m_upResStages.reserve(numScalingStages); // MUST reserve so our pointers won't change
		for (size_t i = 0; i < numScalingStages; i++)
		{
			currentXRes *= 2;
			currentYRes *= 2;

			const string name = "Up-res stage " + to_string(i + 1) + " / " + to_string(numScalingStages);
			re::RenderStage::GraphicsStageParams gfxStageParams;
			m_upResStages.emplace_back(re::RenderStage::CreateGraphicsStage(name, gfxStageParams));

			std::shared_ptr<re::TextureTargetSet> upResTargets = re::TextureTargetSet::Create(name + " targets");

			upResTargets->Viewport().Width() = currentXRes;
			upResTargets->Viewport().Height() = currentYRes;
	
			m_upResStages.back()->AddPermanentParameterBlock(sceneCam->GetCameraParams());
			m_upResStages[i]->SetStageShader(blitShader);

			if (i == (numScalingStages - 1)) // Last iteration: Additive blit back to the src gs
			{
				upResTargets->SetColorTarget(0, deferredLightGS->GetFinalTextureTargetSet()->GetColorTarget(0));

				gr::PipelineState addStageParams(bloomStageParams);
				addStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
				addStageParams.SetSrcBlendMode(gr::PipelineState::BlendMode::One);
				addStageParams.SetDstBlendMode(gr::PipelineState::BlendMode::One);

				m_upResStages.back()->SetStagePipelineState(addStageParams);
			}
			else
			{
				upResTargets->SetColorTarget(0,
					m_downResStages[m_downResStages.size() - (i + 2)]->GetTextureTargetSet()->GetColorTarget(0));

				m_upResStages.back()->SetStagePipelineState(bloomStageParams);
			}

			m_upResStages.back()->SetTextureTargetSet(upResTargets);

			pipeline.AppendRenderStage(m_upResStages[i]);
		}
	}


	void BloomGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();

		GBufferGraphicsSystem* gbufferGS = RenderManager::Get()->GetGraphicsSystem<GBufferGraphicsSystem>();

		DeferredLightingGraphicsSystem* deferredLightGS = 
			RenderManager::Get()->GetGraphicsSystem<DeferredLightingGraphicsSystem>();

		shared_ptr<Sampler> const bloomStageSampler = Sampler::GetSampler(Sampler::WrapAndFilterMode::ClampLinearLinear);

		// This index corresponds with the GBuffer texture layout bindings in SaberCommon.glsl
		// TODO: Have a less brittle way of handling this.
		const size_t gBufferEmissiveTextureIndex = 3; 
		m_emissiveBlitStage->SetPerFrameTextureInput(
			"GBufferAlbedo",
			gbufferGS->GetFinalTextureTargetSet()->GetColorTarget(gBufferEmissiveTextureIndex)->GetTexture(),
			bloomStageSampler);

		for (size_t i = 0; i < m_downResStages.size(); i++)
		{
			if (i == 0)
			{
				m_downResStages[i]->SetPerFrameTextureInput(
					"GBufferAlbedo", 
					m_emissiveBlitStage->GetTextureTargetSet()->GetColorTarget(0)->GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_downResStages[i]->SetPerFrameTextureInput(
					"GBufferAlbedo",
					m_downResStages[i - 1]->GetTextureTargetSet()->GetColorTarget(0)->GetTexture(),
					bloomStageSampler);
			}
		}

		for (size_t i = 0; i < m_blurStages.size(); i++)
		{
			if (i == 0)
			{
				m_blurStages[i]->SetPerFrameTextureInput(
					"GBufferAlbedo",
					m_downResStages.back()->GetTextureTargetSet()->GetColorTarget(0)->GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_blurStages[i]->SetPerFrameTextureInput(
					"GBufferAlbedo",
					m_blurStages[i-1]->GetTextureTargetSet()->GetColorTarget(0)->GetTexture(),
					bloomStageSampler);
			}
		}

		for (size_t i = 0; i < m_upResStages.size(); i++)
		{
			if (i == 0)
			{
				m_upResStages[i]->SetPerFrameTextureInput(
					"GBufferAlbedo",
					m_blurStages.back()->GetTextureTargetSet()->GetColorTarget(0)->GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_upResStages[i]->SetPerFrameTextureInput(
					"GBufferAlbedo",
					m_upResStages[i-1]->GetTextureTargetSet()->GetColorTarget(0)->GetTexture(),
					bloomStageSampler);
			}
		}
	}


	void BloomGraphicsSystem::CreateBatches()
	{
		const Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);

		m_emissiveBlitStage->AddBatch(fullscreenQuadBatch);

		for (size_t i = 0; i < m_downResStages.size(); i++)
		{
			m_downResStages[i]->AddBatch(fullscreenQuadBatch);
		}
		for (size_t i = 0; i < m_blurStages.size(); i++)
		{
			m_blurStages[i]->AddBatch(fullscreenQuadBatch);
		}
		for (size_t i = 0; i < m_upResStages.size(); i++)
		{
			m_upResStages[i]->AddBatch(fullscreenQuadBatch);
		}
	}
}