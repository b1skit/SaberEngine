// © 2022 Adam Badke. All rights reserved.
#include "GraphicsSystem_Bloom.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "Shader.h"
#include "Config.h"
#include "SceneManager.h"
#include "RenderManager.h"
#include "RenderSystem.h"

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
	struct BloomTargetParams
	{
		glm::vec4 g_bloomTargetResolution;

		static constexpr char const* const s_shaderName = "BloomTargetParams";
	};


	BloomTargetParams CreateBloomTargetParamsData(std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		BloomTargetParams bloomTargetParams;
		bloomTargetParams.g_bloomTargetResolution = targetSet->GetTargetDimensions();
		return bloomTargetParams;
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


	void BloomGraphicsSystem::Create(re::RenderSystem& renderSystem, re::StagePipeline& pipeline)
	{
		DeferredLightingGraphicsSystem* deferredLightGS = 
			renderSystem.GetGraphicsSystem<DeferredLightingGraphicsSystem>();

		Camera* sceneCam = SceneManager::Get()->GetMainCamera().get();

		shared_ptr<Shader> blitShader = re::Shader::Create(Config::Get()->GetValue<string>("blitShaderName"));

		// Emissive blit stage:
		gr::PipelineState emissiveStageParams;
		emissiveStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
		emissiveStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		emissiveStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Always);

		m_emissiveBlitStage->SetStagePipelineState(emissiveStageParams);
		m_emissiveBlitStage->SetStageShader(blitShader);
		m_emissiveBlitStage->AddPermanentParameterBlock(sceneCam->GetCameraParams());

		std::shared_ptr<re::TextureTargetSet const> deferredLightGSTargetSet = 
			deferredLightGS->GetFinalTextureTargetSet();
		std::shared_ptr<re::TextureTargetSet> emissiveTargetSet =
			re::TextureTargetSet::Create(*deferredLightGSTargetSet, "Emissive Blit Target Set");

		emissiveTargetSet->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
			re::TextureTarget::TargetParams::BlendMode::One, re::TextureTarget::TargetParams::BlendMode::One});

		m_emissiveBlitStage->SetTextureTargetSet(emissiveTargetSet);
		
		pipeline.AppendRenderStage(m_emissiveBlitStage);

		// Bloom stages:
		gr::PipelineState bloomStageParams;
		bloomStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
		bloomStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		bloomStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Always);
		
		const uint32_t numScalingStages = m_numDownSamplePasses;
		m_downResStages.reserve(numScalingStages); // MUST reserve so our pointers won't change

		int currentXRes = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowXResValueName) / 2;
		int currentYRes = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowYResValueName) / 2;

		// We want the same format as the buffers in the deferred lighting target
		Texture::TextureParams resScaleParams = 
			deferredLightGSTargetSet->GetColorTarget(0).GetTexture()->GetTextureParams();
		resScaleParams.m_width = currentXRes;
		resScaleParams.m_height = currentYRes;
		resScaleParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		re::TextureTarget::TargetParams targetParams;

		shared_ptr<Shader> luminanceThresholdShader =
			re::Shader::Create(Config::Get()->GetValue<string>("luminanceThresholdShaderName"));

		m_bloomParamBlock = re::ParameterBlock::Create(
			BloomParams::s_shaderName,
			m_bloomParams,
			re::ParameterBlock::PBType::Mutable);

		// Downsampling stages (w/luminance threshold in 1st pass):
		for (uint32_t i = 0; i < numScalingStages; i++)
		{
			const string name = "Down-res stage " + to_string(i + 1) + " / " + to_string(numScalingStages);
			re::RenderStage::GraphicsStageParams gfxStageParams;
			m_downResStages.emplace_back(re::RenderStage::CreateGraphicsStage(name, gfxStageParams));

			std::shared_ptr<re::TextureTargetSet> downResTargets = re::TextureTargetSet::Create(name + " targets");
			downResTargets->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
				re::TextureTarget::TargetParams::BlendMode::One,re::TextureTarget::TargetParams::BlendMode::Zero});

			downResTargets->SetViewport(re::Viewport(0, 0, currentXRes, currentYRes));

			resScaleParams.m_width = currentXRes;
			resScaleParams.m_height = currentYRes;
			const string texName = "ScaledResolution_" + to_string(currentXRes) + "x" + to_string(currentYRes);

			downResTargets->SetColorTarget(0, re::Texture::Create(texName, resScaleParams, false), targetParams);

			m_downResStages.back()->SetTextureTargetSet(downResTargets);

			m_downResStages.back()->SetStagePipelineState(bloomStageParams);
			m_downResStages.back()->AddPermanentParameterBlock(sceneCam->GetCameraParams());

			if (i == 0)
			{
				m_downResStages[i]->SetStageShader(luminanceThresholdShader);

				m_downResStages[i]->AddPermanentParameterBlock(m_bloomParamBlock);
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

			blurTargets->SetViewport(re::Viewport(0, 0, currentXRes, currentYRes));

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
				BloomTargetParams::s_shaderName,
				CreateBloomTargetParamsData(blurTargets),
				re::ParameterBlock::PBType::Immutable));

			pipeline.AppendRenderStage(m_blurStages[i]);
		}

		// Up-res stages:
		gr::PipelineState upresStageParams;
		upresStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
		upresStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		upresStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Always);

		m_upResStages.reserve(numScalingStages); // MUST reserve so our pointers won't change
		for (size_t i = 0; i < numScalingStages; i++)
		{
			currentXRes *= 2;
			currentYRes *= 2;

			const string name = "Up-res stage " + to_string(i + 1) + " / " + to_string(numScalingStages);
			re::RenderStage::GraphicsStageParams gfxStageParams;
			m_upResStages.emplace_back(re::RenderStage::CreateGraphicsStage(name, gfxStageParams));

			std::shared_ptr<re::TextureTargetSet> upResTargets = re::TextureTargetSet::Create(name + " targets");

			upResTargets->SetViewport(re::Viewport(0, 0, currentXRes, currentYRes));

			m_upResStages.back()->AddPermanentParameterBlock(sceneCam->GetCameraParams());
			m_upResStages[i]->SetStageShader(blitShader);

			if (i == (numScalingStages - 1)) // Last iteration: Additive blit back to the src gs
			{
				upResTargets->SetColorTarget(0, deferredLightGS->GetFinalTextureTargetSet()->GetColorTarget(0));

				upResTargets->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
					re::TextureTarget::TargetParams::BlendMode::One, re::TextureTarget::TargetParams::BlendMode::One });

				gr::PipelineState addStageParams(upresStageParams);
				addStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);

				m_upResStages.back()->SetStagePipelineState(addStageParams);
			}
			else
			{
				upResTargets->SetColorTarget(0,
					m_downResStages[m_downResStages.size() - (i + 2)]->GetTextureTargetSet()->GetColorTarget(0));

				upResTargets->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
					re::TextureTarget::TargetParams::BlendMode::One, re::TextureTarget::TargetParams::BlendMode::Zero});

				m_upResStages.back()->SetStagePipelineState(upresStageParams);
			}

			m_upResStages.back()->SetTextureTargetSet(upResTargets);

			pipeline.AppendRenderStage(m_upResStages[i]);
		}

		// Attach GBuffer inputs:
		GBufferGraphicsSystem* gbufferGS = renderSystem.GetGraphicsSystem<GBufferGraphicsSystem>();

		shared_ptr<Sampler> const bloomStageSampler = Sampler::GetSampler(Sampler::WrapAndFilterMode::ClampLinearLinear);

		// This index corresponds with the GBuffer texture layout bindings in SaberCommon.glsl
		const size_t gBufferEmissiveTextureSrcIndex = 3;
		m_emissiveBlitStage->AddTextureInput(
			"Tex0",
			gbufferGS->GetFinalTextureTargetSet()->GetColorTarget(gBufferEmissiveTextureSrcIndex).GetTexture(),
			bloomStageSampler);

		for (size_t i = 0; i < m_downResStages.size(); i++)
		{
			if (i == 0)
			{
				m_downResStages[i]->AddTextureInput(
					"Tex0",
					m_emissiveBlitStage->GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_downResStages[i]->AddTextureInput(
					"Tex0",
					m_downResStages[i - 1]->GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
		}

		for (size_t i = 0; i < m_blurStages.size(); i++)
		{
			if (i == 0)
			{
				m_blurStages[i]->AddTextureInput(
					"Tex0",
					m_downResStages.back()->GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_blurStages[i]->AddTextureInput(
					"Tex0",
					m_blurStages[i - 1]->GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
		}

		for (size_t i = 0; i < m_upResStages.size(); i++)
		{
			if (i == 0)
			{
				m_upResStages[i]->AddTextureInput(
					"Tex0",
					m_blurStages.back()->GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
			else
			{
				m_upResStages[i]->AddTextureInput(
					"Tex0",
					m_upResStages[i - 1]->GetTextureTargetSet()->GetColorTarget(0).GetTexture(),
					bloomStageSampler);
			}
		}
	}


	void BloomGraphicsSystem::PreRender()
	{
		CreateBatches();

		// Update our bloom params:
		m_bloomParamBlock->Commit(m_bloomParams);
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


	void BloomGraphicsSystem::ShowImGuiWindow()
	{
		if (ImGui::TreeNode(GetName().c_str()))
		{
			auto ShowTooltip = [&]() {
				if (ImGui::BeginItemTooltip())
				{
					ImGui::Text("Luminance threshold sigmoid");
					constexpr uint32_t k_numSamples = 20;
					constexpr float k_sampleSpacing = 0.2f;
					float samplePoints[k_numSamples];
					const float sigmoidPower = m_bloomParams.g_sigmoidParams.x;
					const float sigmoidSpeed = m_bloomParams.g_sigmoidParams.y;
					for (size_t i = 0; i < k_numSamples; i++)
					{
						const float x = i * k_sampleSpacing;

						const float commonTerm = std::pow((sigmoidSpeed * x), sigmoidPower);
						samplePoints[i] = commonTerm / (commonTerm + 1);
					}
					ImGui::PlotLines("Curve", samplePoints, k_numSamples);

					ImGui::EndTooltip();
				}
			};

			ImGui::SliderFloat("Sigmoid ramp power", &m_bloomParams.g_sigmoidParams.x, 0, 15.0f, "Sigmoid ramp power = %.3f");
			ShowTooltip();

			ImGui::SliderFloat("Sigmoid ramp speed", &m_bloomParams.g_sigmoidParams.y, 0, 5.0f, "Sigmoid ramp speed = %.3f");
			ShowTooltip();

			ImGui::TreePop();
		}
	}
}