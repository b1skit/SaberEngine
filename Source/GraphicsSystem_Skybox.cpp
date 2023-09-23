// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "Texture.h"
#include "TextureTarget.h"

using en::Config;
using en::SceneManager;
using gr::DeferredLightingGraphicsSystem;
using gr::GBufferGraphicsSystem;
using re::RenderManager;
using re::RenderStage;
using re::Sampler;
using re::Batch;
using re::TextureTargetSet;
using re::Texture;
using re::Shader;
using std::shared_ptr;
using std::string;
using std::vector;
using std::filesystem::exists;
using glm::vec3;
using glm::vec4;
using glm::mat4;


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


	SkyboxGraphicsSystem::SkyboxGraphicsSystem()
		: GraphicsSystem(k_gsName)
		, NamedObject(k_gsName)
		, m_skyTexture(nullptr)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_skyboxStage = re::RenderStage::CreateGraphicsStage("Skybox stage", gfxStageParams);

		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Far);
	}


	void SkyboxGraphicsSystem::Create(re::RenderSystem& renderSystem, re::StagePipeline& pipeline)
	{
		// Create a skybox shader, now that we have some sort of image loaded:
		m_skyboxStage->SetStageShader(re::Shader::Create(en::ShaderNames::k_skyboxShaderName));

		// Load the HDR image:
		m_skyTexture = SceneManager::GetSceneData()->GetIBLTexture();
		m_skyTextureShaderName = "Tex0";

		gr::PipelineState skyboxStageParams;
		skyboxStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
		skyboxStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back);
		skyboxStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::LEqual);

		m_skyboxStage->SetStagePipelineState(skyboxStageParams);

		m_skyboxStage->AddPermanentParameterBlock(SceneManager::Get()->GetMainCamera()->GetCameraParams());

		DeferredLightingGraphicsSystem* deferredLightGS = 
			renderSystem.GetGraphicsSystem<DeferredLightingGraphicsSystem>();

		// Create a new texture target set so we can write to the deferred lighting color targets, but attach the
		// GBuffer depth for HW depth testing
		std::shared_ptr<re::TextureTargetSet> skyboxTargets = 
			re::TextureTargetSet::Create(*deferredLightGS->GetFinalTextureTargetSet(), "Skybox Targets");

		re::TextureTarget::TargetParams depthTargetParams;
		depthTargetParams.m_channelWriteMode.R = re::TextureTarget::TargetParams::ChannelWrite::Disabled;

		GBufferGraphicsSystem* gBufferGS = renderSystem.GetGraphicsSystem<GBufferGraphicsSystem>();
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
			Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));

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
		const Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);
		m_skyboxStage->AddBatch(fullscreenQuadBatch);
	}
}