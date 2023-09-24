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


	struct GaussianBlurParams
	{
		glm::vec4 g_blurSettings; // .x = Bloom direction (0 = horizontal, 1 = vertical), .yzw = unused

		static constexpr char const* const s_shaderName = "GaussianBlurParams";
	};

	enum class BloomDirection
	{
		Horizontal = 0,
		Vertical = 1
	};

	GaussianBlurParams CreateBloomParamsData(BloomDirection bloomDirection)
	{
		const float bloomDir = static_cast<float>(bloomDirection);

		GaussianBlurParams bloomParams;
		bloomParams.g_blurSettings = glm::vec4(bloomDir, 0.f, 0.f, 0.f);;
		return bloomParams;
	}
}

namespace gr
{
	constexpr char const* k_gsName = "Bloom Graphics System";


	BloomGraphicsSystem::BloomGraphicsSystem()
		: GraphicsSystem(k_gsName)
		, NamedObject(k_gsName)
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

		shared_ptr<Shader> blitShader = re::Shader::Create(en::ShaderNames::k_blitShaderName);

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
			re::Shader::Create(en::ShaderNames::k_luminanceThresholdShaderName);

		// Create our param blocks:
		m_luminanceThresholdParamBlock = re::ParameterBlock::Create(
			LuminanceThresholdParams::s_shaderName,
			m_luminanceThresholdParams,
			re::ParameterBlock::PBType::Mutable);

		// Downsampling stages (w/luminance threshold in 1st pass):
		for (uint32_t i = 0; i < m_numDownSamplePasses; i++)
		{
			const string name = "Down-res stage " + to_string(i + 1) + " / " + to_string(m_numDownSamplePasses);
			re::RenderStage::GraphicsStageParams gfxStageParams;
			m_downResStages[i] = re::RenderStage::CreateGraphicsStage(name, gfxStageParams);
			re::RenderStage* downResStage = m_downResStages[i].get();

			std::shared_ptr<re::TextureTargetSet> downResTargets = re::TextureTargetSet::Create(name + " targets");
			downResTargets->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
				re::TextureTarget::TargetParams::BlendMode::One,re::TextureTarget::TargetParams::BlendMode::Zero});

			downResTargets->SetViewport(re::Viewport(0, 0, currentXRes, currentYRes));
			downResTargets->SetScissorRect(re::ScissorRect(0, 0, currentXRes, currentYRes));

			resScaleParams.m_width = currentXRes;
			resScaleParams.m_height = currentYRes;
			const string texName = "ScaledResolution_" + to_string(currentXRes) + "x" + to_string(currentYRes);

			downResTargets->SetColorTarget(0, re::Texture::Create(texName, resScaleParams, false), targetParams);

			downResStage->SetTextureTargetSet(downResTargets);

			downResStage->SetStagePipelineState(bloomStageParams);
			downResStage->AddPermanentParameterBlock(sceneCam->GetCameraParams());

			if (i == 0)
			{
				m_downResStages[i]->SetStageShader(luminanceThresholdShader);

				m_downResStages[i]->AddPermanentParameterBlock(m_luminanceThresholdParamBlock);
			}
			else
			{
				m_downResStages[i]->SetStageShader(blitShader);
			}

			pipeline.AppendRenderStage(m_downResStages[i]);

			// Don't halve the resolution on the last iteration:
			if (i < (m_numDownSamplePasses - 1))
			{
				currentXRes /= 2;
				currentYRes /= 2;
			}
		}

		// Blur stages:
		shared_ptr<Shader> gaussianBlurShader =
			re::Shader::Create(en::ShaderNames::k_gaussianBlurShaderName);

		// Create our param blocks:
		m_horizontalBloomParams = re::ParameterBlock::Create(
			GaussianBlurParams::s_shaderName,
			CreateBloomParamsData(BloomDirection::Horizontal),
			re::ParameterBlock::PBType::Immutable);

		m_verticalBloomParams = re::ParameterBlock::Create(
			GaussianBlurParams::s_shaderName,
			CreateBloomParamsData(BloomDirection::Vertical),
			re::ParameterBlock::PBType::Immutable);

		Texture::TextureParams blurParams(resScaleParams);
		blurParams.m_width = currentXRes;
		blurParams.m_height = currentYRes;
		const string texName = "BlurPingPong_" + to_string(currentXRes) + "x" + to_string(currentYRes);
		
		shared_ptr<Texture> blurPingPongTexture = re::Texture::Create(texName, blurParams, false);

		const uint32_t totalBlurPasses = m_numBlurPasses * 2; // x2 for horizontal/vertical separation

		constexpr char const* blurStageNamePrefix[2] = { "Horizontal", "Vertical"};

		for (size_t i = 0; i < totalBlurPasses; i++)
		{
			std::string const& stageName = 
				std::format("{} blur stage {} / {}", blurStageNamePrefix[i % 2], (i + 2) / 2, m_numBlurPasses);

			re::RenderStage::GraphicsStageParams gfxStageParams;
			m_blurStages[i] = re::RenderStage::CreateGraphicsStage(stageName, gfxStageParams);
			re::RenderStage* newBlurStage = m_blurStages[i].get();

			std::shared_ptr<re::TextureTargetSet> blurTargets = re::TextureTargetSet::Create(stageName + " targets");
			blurTargets->SetViewport(re::Viewport(0, 0, currentXRes, currentYRes));
			blurTargets->SetScissorRect(re::ScissorRect(0, 0, currentXRes, currentYRes));

			newBlurStage->SetStagePipelineState(bloomStageParams);
			newBlurStage->AddPermanentParameterBlock(sceneCam->GetCameraParams());

			if (i % 2 == 0)
			{
				blurTargets->SetColorTarget(0, blurPingPongTexture, targetParams);
				newBlurStage->SetStageShader(gaussianBlurShader);

				newBlurStage->AddPermanentParameterBlock(m_horizontalBloomParams);
			}
			else
			{
				blurTargets->SetColorTarget(0, m_downResStages.back()->GetTextureTargetSet()->GetColorTarget(0));
				newBlurStage->SetStageShader(gaussianBlurShader);

				newBlurStage->AddPermanentParameterBlock(m_verticalBloomParams);
			}
			newBlurStage->SetTextureTargetSet(blurTargets);

			newBlurStage->AddPermanentParameterBlock(re::ParameterBlock::Create(
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

		for (size_t i = 0; i < m_numDownSamplePasses; i++)
		{
			currentXRes *= 2;
			currentYRes *= 2;

			const string name = "Up-res stage " + to_string(i + 1) + " / " + to_string(m_numDownSamplePasses);
			re::RenderStage::GraphicsStageParams gfxStageParams;
			m_upResStages[i] = re::RenderStage::CreateGraphicsStage(name, gfxStageParams);
			re::RenderStage* upresStage = m_upResStages[i].get();

			std::shared_ptr<re::TextureTargetSet> upResTargets = re::TextureTargetSet::Create(name + " targets");

			upResTargets->SetViewport(re::Viewport(0, 0, currentXRes, currentYRes));
			upResTargets->SetScissorRect(re::ScissorRect(0, 0, currentXRes, currentYRes));

			upresStage->AddPermanentParameterBlock(sceneCam->GetCameraParams());
			m_upResStages[i]->SetStageShader(blitShader);

			if (i == (m_numDownSamplePasses - 1)) // Last iteration: Additive blit back to the src gs
			{
				upResTargets->SetColorTarget(0, deferredLightGS->GetFinalTextureTargetSet()->GetColorTarget(0));

				upResTargets->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
					re::TextureTarget::TargetParams::BlendMode::One, re::TextureTarget::TargetParams::BlendMode::One });

				gr::PipelineState addStageParams(upresStageParams);
				addStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);

				upresStage->SetStagePipelineState(addStageParams);
			}
			else
			{
				upResTargets->SetColorTarget(0,
					m_downResStages[m_downResStages.size() - (i + 2)]->GetTextureTargetSet()->GetColorTarget(0));

				upResTargets->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
					re::TextureTarget::TargetParams::BlendMode::One, re::TextureTarget::TargetParams::BlendMode::Zero});

				upresStage->SetStagePipelineState(upresStageParams);
			}

			upresStage->SetTextureTargetSet(upResTargets);

			pipeline.AppendRenderStage(m_upResStages[i]);
		}

		// Attach GBuffer inputs:
		GBufferGraphicsSystem* gbufferGS = renderSystem.GetGraphicsSystem<GBufferGraphicsSystem>();

		shared_ptr<Sampler> const bloomStageSampler = Sampler::GetSampler(Sampler::WrapAndFilterMode::Clamp_Linear_Linear);

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
		m_luminanceThresholdParamBlock->Commit(m_luminanceThresholdParams);
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
					const float sigmoidPower = m_luminanceThresholdParams.g_sigmoidParams.x;
					const float sigmoidSpeed = m_luminanceThresholdParams.g_sigmoidParams.y;
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

			ImGui::SliderFloat("Sigmoid ramp power", &m_luminanceThresholdParams.g_sigmoidParams.x, 0, 15.0f, "Sigmoid ramp power = %.3f");
			ShowTooltip();

			ImGui::SliderFloat("Sigmoid ramp speed", &m_luminanceThresholdParams.g_sigmoidParams.y, 0, 5.0f, "Sigmoid ramp speed = %.3f");
			ShowTooltip();

			ImGui::TreePop();
		}
	}
}