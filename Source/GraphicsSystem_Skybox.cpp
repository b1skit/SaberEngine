// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "MeshFactory.h"
#include "RenderManager.h"
#include "Sampler.h"
#include "SceneManager.h"
#include "Texture.h"
#include "TextureTarget.h"


namespace
{
	struct SkyboxParams
	{
		glm::vec4 g_skyboxTargetResolution;

		static constexpr char const* const s_shaderName = "SkyboxParams";
	};


	SkyboxParams CreateSkyboxParamsData(std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		SkyboxParams skyboxParams;
		skyboxParams.g_skyboxTargetResolution = targetSet->GetTargetDimensions();
		return skyboxParams;
	}
}

namespace gr
{
	constexpr char const* k_gsName = "Skybox Graphics System";


	SkyboxGraphicsSystem::SkyboxGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, NamedObject(k_gsName)
		, m_skyTexture(nullptr)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_skyboxStage = re::RenderStage::CreateGraphicsStage("Skybox stage", gfxStageParams);

		m_screenAlignedQuad = gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Far);
	}


	void SkyboxGraphicsSystem::Create(re::RenderSystem& renderSystem, re::StagePipeline& pipeline)
	{
		re::PipelineState skyboxPipelineState;
		skyboxPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);
		skyboxPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::LEqual);

		m_skyboxStage->SetStageShader(re::Shader::GetOrCreate(en::ShaderNames::k_skyboxShaderName, skyboxPipelineState));

		// Load the HDR image:
		m_skyTexture = fr::SceneManager::GetSceneData()->GetIBLTexture();
		m_skyTextureShaderName = "Tex0";

		m_skyboxStage->AddPermanentParameterBlock(m_graphicsSystemManager->GetActiveCameraParams());

		DeferredLightingGraphicsSystem* deferredLightGS = 
			m_graphicsSystemManager->GetGraphicsSystem<DeferredLightingGraphicsSystem>();

		// Create a new texture target set so we can write to the deferred lighting color targets, but attach the
		// GBuffer depth for HW depth testing
		std::shared_ptr<re::TextureTargetSet> skyboxTargets = 
			re::TextureTargetSet::Create(*deferredLightGS->GetFinalTextureTargetSet(), "Skybox Targets");
		skyboxTargets->SetAllTargetClearModes(re::TextureTarget::TargetParams::ClearMode::Disabled);

		re::TextureTarget::TargetParams depthTargetParams;
		depthTargetParams.m_channelWriteMode.R = re::TextureTarget::TargetParams::ChannelWrite::Disabled;

		GBufferGraphicsSystem* gBufferGS = m_graphicsSystemManager->GetGraphicsSystem<GBufferGraphicsSystem>();
		skyboxTargets->SetDepthStencilTarget(
			gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
			depthTargetParams);

		// Render on top of the frame
		const re::TextureTarget::TargetParams::BlendModes skyboxBlendModes
		{
			re::TextureTarget::TargetParams::BlendMode::One,
			re::TextureTarget::TargetParams::BlendMode::Zero
		};
		skyboxTargets->SetColorTargetBlendModes(1, &skyboxBlendModes);

		m_skyboxStage->SetTextureTargetSet(skyboxTargets);

		m_skyboxStage->AddPermanentParameterBlock(re::ParameterBlock::Create(
			SkyboxParams::s_shaderName,
			CreateSkyboxParamsData(skyboxTargets),
			re::ParameterBlock::PBType::Immutable));

		m_skyboxStage->AddTextureInput(
			m_skyTextureShaderName,
			m_skyTexture,
			re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear));

		pipeline.AppendRenderStage(m_skyboxStage);
	}


	void SkyboxGraphicsSystem::PreRender()
	{
		CreateBatches();
	}


	std::shared_ptr<re::TextureTargetSet const> SkyboxGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_skyboxStage->GetTextureTargetSet();
	}


	void SkyboxGraphicsSystem::CreateBatches()
	{
		const re::Batch fullscreenQuadBatch = 
			re::Batch(re::Batch::Lifetime::SingleFrame, m_screenAlignedQuad.get(), nullptr);
		m_skyboxStage->AddBatch(fullscreenQuadBatch);
	}
}