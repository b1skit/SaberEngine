// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Shadows.h"
#include "Light.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "ShadowMap.h"


namespace
{
	struct CubemapShadowRenderParams
	{
		glm::mat4 g_cubemapShadowCam_VP[6];
		glm::vec4 g_cubemapShadowCamNearFar; // .xy = near, far. .zw = unused
		glm::vec4 g_cubemapLightWorldPos; // .xyz = light word pos, .w = unused

		static constexpr char const* const s_shaderName = "CubemapShadowRenderParams"; // Not counted towards size of struct
	};


	CubemapShadowRenderParams GetCubemapShadowRenderParamsData(fr::Camera* shadowCam)
	{
		// ECS_CONVSERSION TODO: Construct this from camera render data

		SEAssert("Shadow camera is not configured for cubemap rendering", 
			shadowCam->GetCameraConfig().m_projectionType == gr::Camera::Config::ProjectionType::PerspectiveCubemap);

		CubemapShadowRenderParams cubemapShadowParams{};

		memcpy(&cubemapShadowParams.g_cubemapShadowCam_VP[0][0].x,
			shadowCam->GetCubeViewProjectionMatrices().data(),
			6 * sizeof(glm::mat4));

		cubemapShadowParams.g_cubemapShadowCamNearFar = glm::vec4(shadowCam->GetNearFar().xy, 0.f, 0.f);
		cubemapShadowParams.g_cubemapLightWorldPos = glm::vec4(shadowCam->GetTransform()->GetGlobalPosition(), 0.f);

		return cubemapShadowParams;
	}
}


namespace gr
{
	constexpr char const* k_gsName = "Shadows Graphics System";


	ShadowsGraphicsSystem::ShadowsGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, NamedObject(k_gsName)
		, m_hasDirectionalLight(false)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_directionalShadowStage = re::RenderStage::CreateGraphicsStage("Keylight shadow", gfxStageParams);

		m_directionalShadowStage->SetBatchFilterMaskBit(re::Batch::Filter::NoShadow);
	}


	void ShadowsGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		re::PipelineState shadowPipelineState;
		
		// TODO: FaceCullingMode::Disabled is better for minimizing peter-panning, but we need backface culling if we
		// want to be able to place lights inside of geometry (eg. emissive spheres). For now, enable backface culling.
		// In future, we need to support tagging assets to not cast shadows
		shadowPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);

		shadowPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Less);

		// Directional light shadow:		
		std::shared_ptr<fr::Light> directionalLight = en::SceneManager::GetSceneData()->GetKeyLight();
		if (directionalLight)
		{
			m_hasDirectionalLight = true;

			fr::ShadowMap* const directionalShadow = directionalLight->GetShadowMap();
			if (directionalShadow)
			{
				m_directionalShadowStage->AddPermanentParameterBlock(
					directionalShadow->ShadowCamera()->GetCameraParams());

				// Shader:
				m_directionalShadowStage->SetStageShader(
					re::Shader::GetOrCreate(en::ShaderNames::k_depthShaderName, shadowPipelineState));

				std::shared_ptr<re::TextureTargetSet> directionalShadowTargetSet = 
					directionalLight->GetShadowMap()->GetTextureTargetSet();

				directionalShadowTargetSet->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
					re::TextureTarget::TargetParams::BlendMode::Disabled,
					re::TextureTarget::TargetParams::BlendMode::Disabled});

				directionalShadowTargetSet->SetAllColorWriteModes(re::TextureTarget::TargetParams::ChannelWrite{
					re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
					re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
					re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
					re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled });
					
				directionalShadowTargetSet->SetDepthWriteMode(
					re::TextureTarget::TargetParams::ChannelWrite::Mode::Enabled);

				directionalShadowTargetSet->SetDepthTargetClearMode(re::TextureTarget::TargetParams::ClearMode::Enabled);

				m_directionalShadowStage->SetTextureTargetSet(directionalShadowTargetSet);
				// TODO: Target set should be a member of the stage, instead of the shadow map?
				// -> HARD: The stages are already created, we don't know what lights are associated with each stage

				pipeline.AppendRenderStage(m_directionalShadowStage);
			}
		}
		
		// Point light shadows:
		std::vector<std::shared_ptr<fr::Light>> const& deferredLights = en::SceneManager::GetSceneData()->GetPointLights();

		m_pointLightShadowStageCams.reserve(deferredLights.size());
		m_cubemapShadowParamBlocks.reserve(deferredLights.size());

		for (std::shared_ptr<fr::Light> const& curLight : deferredLights)
		{
			re::RenderStage::GraphicsStageParams gfxStageParams;
			m_pointLightShadowStages.emplace_back(
				re::RenderStage::CreateGraphicsStage(curLight->GetName() + " shadow", gfxStageParams));

			std::shared_ptr<re::RenderStage> shadowStage = m_pointLightShadowStages.back();

			shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::NoShadow);
			
			fr::ShadowMap* const lightShadow = curLight->GetShadowMap();
			if (lightShadow)
			{
				fr::Camera* const shadowCam = lightShadow->ShadowCamera();
				shadowStage->AddPermanentParameterBlock(shadowCam->GetCameraParams());
				
				m_pointLightShadowStageCams.emplace_back(shadowCam);

				// Shader:
				shadowStage->SetStageShader(
					re::Shader::GetOrCreate(en::ShaderNames::k_cubeDepthShaderName, shadowPipelineState));

				std::shared_ptr<re::TextureTargetSet> pointShadowTargetSet = 
					curLight->GetShadowMap()->GetTextureTargetSet();

				pointShadowTargetSet->SetDepthTargetClearMode(re::TextureTarget::TargetParams::ClearMode::Enabled);

				shadowStage->SetTextureTargetSet(pointShadowTargetSet);

				// Cubemap shadow param block:
				CubemapShadowRenderParams cubemapShadowParams = GetCubemapShadowRenderParamsData(shadowCam);
				
				// BUG HERE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				// -> Calling GetCubemapShadowRenderParamsData during this function causes point light shadows to be
				// slightly different?!?!?!?!
				// -> They work if we call it to set a single frame PB in PreRender?!?!
				// -> Probably something to do with the shadowCam->GetTransform()->GetGlobalPosition()
				//		-> The only non-const call...
				//		-> Probably will be fixed when we pass transform data to the render thread????????????????????

				m_cubemapShadowParamBlocks.emplace_back(re::ParameterBlock::Create(
					CubemapShadowRenderParams::s_shaderName,
					cubemapShadowParams,
					re::ParameterBlock::PBType::Mutable));

				shadowStage->AddPermanentParameterBlock(m_cubemapShadowParamBlocks.back());



				pipeline.AppendRenderStage(shadowStage);
			}
			else
			{
				m_pointLightShadowStageCams.emplace_back(nullptr); // Ensure we stay in sync: 1 entry per stage
				m_cubemapShadowParamBlocks.emplace_back(nullptr);
			}
		}
	}


	void ShadowsGraphicsSystem::PreRender()
	{
		CreateBatches();
		
		for (size_t pointLightStageIdx = 0; pointLightStageIdx < m_pointLightShadowStages.size(); pointLightStageIdx++)
		{
			fr::Camera* shadowCam = m_pointLightShadowStageCams[pointLightStageIdx];
			if (shadowCam)
			{
				// Update the param block data:
				CubemapShadowRenderParams const& cubemapShadowParams = GetCubemapShadowRenderParamsData(shadowCam);

				m_cubemapShadowParamBlocks[pointLightStageIdx]->Commit(cubemapShadowParams);

				//shared_ptr<re::ParameterBlock> cubemapShadowPB = re::ParameterBlock::Create(
				//	CubemapShadowRenderParams::s_shaderName,
				//	cubemapShadowParams,
				//	re::ParameterBlock::PBType::SingleFrame);

				//m_pointLightShadowStages[pointLightStageIdx]->AddSingleFrameParameterBlock(cubemapShadowPB);
			}
		}
	}


	void ShadowsGraphicsSystem::CreateBatches()
	{
		// TODO: Create batches specific to this GS: Cached, culled, and with only the appropriate PBs etc attached
		if (m_hasDirectionalLight)
		{
			m_directionalShadowStage->AddBatches(re::RenderManager::Get()->GetSceneBatches());
		}

		for (std::shared_ptr<re::RenderStage> pointShadowStage : m_pointLightShadowStages)
		{
			pointShadowStage->AddBatches(re::RenderManager::Get()->GetSceneBatches());
		}
	}


	std::shared_ptr<re::TextureTargetSet const> ShadowsGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_directionalShadowStage->GetTextureTargetSet();
	}
}