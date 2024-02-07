// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Skybox.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "MeshFactory.h"
#include "RenderManager.h"
#include "Sampler.h"
#include "SceneManager.h"
#include "Shader.h"
#include "Texture.h"
#include "TextureTarget.h"


namespace
{
	struct SkyboxParams
	{
		glm::vec4 g_backgroundColorIsEnabled; // .rgb = background color override, .a = enabled/disabled (1.f/0.f)

		static constexpr char const* const s_shaderName = "SkyboxParams";
	};


	SkyboxParams CreateSkyboxParamsData(glm::vec3 const& backgroundColor, bool showBackgroundColor)
	{
		SkyboxParams skyboxParams;
		skyboxParams.g_backgroundColorIsEnabled = glm::vec4(backgroundColor.rgb, static_cast<float>(showBackgroundColor));
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
		, m_backgroundColor(135.f / 255.f, 206.f / 255.f, 235.f / 255.f)
		, m_showBackgroundColor(false)
		, m_isDirty(true)
	{
	}


	void SkyboxGraphicsSystem::Create(re::RenderSystem& renderSystem, re::StagePipeline& pipeline)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_skyboxStage = re::RenderStage::CreateGraphicsStage("Skybox stage", gfxStageParams);

		if (m_screenAlignedQuad == nullptr)
		{
			m_screenAlignedQuad = gr::meshfactory::CreateFullscreenQuad(gr::meshfactory::ZLocation::Far);
		}

		re::PipelineState skyboxPipelineState;
		skyboxPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);
		skyboxPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::LEqual);

		m_skyboxStage->SetStageShader(re::Shader::GetOrCreate(en::ShaderNames::k_skyboxShaderName, skyboxPipelineState));

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		SEAssert(renderData.GetNumElementsOfType<gr::Light::RenderDataAmbientIBL>() == 1,
			"We currently expect render data for exactly 1 ambient light to exist");

		gr::RenderDataID ambientID = renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataAmbientIBL>()[0];

		gr::Light::RenderDataAmbientIBL const& ambientRenderData = 
			renderData.GetObjectData<gr::Light::RenderDataAmbientIBL>(ambientID);

		m_skyTexture = ambientRenderData.m_iblTex;

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

		m_skyboxParams = re::ParameterBlock::Create(
			SkyboxParams::s_shaderName,
			CreateSkyboxParamsData(m_backgroundColor, m_showBackgroundColor),
			re::ParameterBlock::PBType::Mutable);

		m_skyboxStage->AddPermanentParameterBlock(m_skyboxParams);

		constexpr char const* k_skyboxTexShaderName = "Tex0";
		m_skyboxStage->AddTextureInput(
			k_skyboxTexShaderName,
			m_skyTexture,
			re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear));

		pipeline.AppendRenderStage(m_skyboxStage);
	}


	void SkyboxGraphicsSystem::PreRender()
	{
		if (m_isDirty)
		{
			m_skyboxParams->Commit(CreateSkyboxParamsData(m_backgroundColor, m_showBackgroundColor));
			m_isDirty = false;
		}

		CreateBatches();
	}


	std::shared_ptr<re::TextureTargetSet const> SkyboxGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_skyboxStage->GetTextureTargetSet();
	}


	void SkyboxGraphicsSystem::CreateBatches()
	{
		if (m_fullscreenQuadBatch == nullptr)
		{
			m_fullscreenQuadBatch = std::make_unique<re::Batch>(re::Batch::Lifetime::Permanent, m_screenAlignedQuad.get());
		}

		m_skyboxStage->AddBatch(*m_fullscreenQuadBatch);
	}


	void SkyboxGraphicsSystem::ShowImGuiWindow()
	{
		m_isDirty |= ImGui::Checkbox("Use flat background color", &m_showBackgroundColor);
		m_isDirty |= ImGui::ColorEdit3("Background color", &m_backgroundColor.r);
	}
}