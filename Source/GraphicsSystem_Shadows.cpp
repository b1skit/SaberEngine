// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Shadows.h"
#include "Light.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "ShadowMap.h"


using en::SceneManager;
using en::Config;
using re::RenderManager;
using gr::Light;
using gr::ShadowMap;
using re::RenderStage;
using re::Shader;
using std::vector;
using std::string;
using std::shared_ptr;
using std::make_shared;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;


namespace
{
	struct CubemapShadowRenderParams
	{
		glm::mat4 g_cubemapShadowCam_VP[6];
		glm::vec4 g_cubemapShadowCamNearFar; // .xy = near, far. .zw = unused
		glm::vec4 g_cubemapLightWorldPos; // .xyz = light word pos, .w = unused

		static constexpr char const* const s_shaderName = "CubemapShadowRenderParams"; // Not counted towards size of struct
	};


	CubemapShadowRenderParams GetCubemapShadowRenderParamsData(gr::Camera* shadowCam)
	{
		CubemapShadowRenderParams cubemapShadowParams;
		memcpy(&cubemapShadowParams.g_cubemapShadowCam_VP[0][0].x,
			shadowCam->GetCubeViewProjectionMatrix().data(),
			6 * sizeof(mat4));

		cubemapShadowParams.g_cubemapShadowCamNearFar = glm::vec4(shadowCam->NearFar().xy, 0.f, 0.f);
		cubemapShadowParams.g_cubemapLightWorldPos = glm::vec4(shadowCam->GetTransform()->GetGlobalPosition(), 0.f);

		return cubemapShadowParams;
	}
}


namespace gr
{
	constexpr char const* k_gsName = "Shadows Graphics System";


	ShadowsGraphicsSystem::ShadowsGraphicsSystem()
		: GraphicsSystem(k_gsName)
		, NamedObject(k_gsName)
		, m_hasDirectionalLight(false)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_directionalShadowStage = re::RenderStage::CreateGraphicsStage("Keylight shadow", gfxStageParams);
	}


	void ShadowsGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		re::PipelineState shadowStageParams;
		shadowStageParams.SetClearTarget(re::PipelineState::ClearTarget::Depth);
		
		// TODO: FaceCullingMode::Disabled is better for minimizing peter-panning, but we need backface culling if we
		// want to be able to place lights inside of geometry (eg. emissive spheres). For now, enable backface culling.
		// In future, we need to support tagging assets to not cast shadows
		shadowStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);

		shadowStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::Less);

		// Directional light shadow:		
		shared_ptr<Light> directionalLight = SceneManager::GetSceneData()->GetKeyLight();
		if (directionalLight)
		{
			m_hasDirectionalLight = true;

			ShadowMap* const directionalShadow = directionalLight->GetShadowMap();
			if (directionalShadow)
			{
				m_directionalShadowStage->AddPermanentParameterBlock(
					directionalShadow->ShadowCamera()->GetCameraParams());

				// Shader:
				m_directionalShadowStage->SetStageShader(
					re::Shader::Create(en::ShaderNames::k_depthShaderName));

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

				m_directionalShadowStage->SetTextureTargetSet(directionalShadowTargetSet);
				// TODO: Target set should be a member of the stage, instead of the shadow map?
				// -> HARD: The stages are already created, we don't know what lights are associated with each stage

				m_directionalShadowStage->SetStagePipelineState(shadowStageParams);

				pipeline.AppendRenderStage(m_directionalShadowStage);
			}
		}
		
		// Point light shadows:
		vector<shared_ptr<Light>> const& deferredLights = SceneManager::GetSceneData()->GetPointLights();
		m_pointLightShadowStageCams.reserve(deferredLights.size());
		for (shared_ptr<Light> curLight : deferredLights)
		{
			re::RenderStage::GraphicsStageParams gfxStageParams;
			m_pointLightShadowStages.emplace_back(
				re::RenderStage::CreateGraphicsStage(curLight->GetName() + " shadow", gfxStageParams));

			std::shared_ptr<re::RenderStage> shadowStage = m_pointLightShadowStages.back();
			
			ShadowMap* const lightShadow = curLight->GetShadowMap();
			if (lightShadow)
			{
				Camera* const shadowCam = lightShadow->ShadowCamera();
				shadowStage->AddPermanentParameterBlock(shadowCam->GetCameraParams());
				m_pointLightShadowStageCams.emplace_back(shadowCam);

				// Shader:
				shadowStage->SetStageShader(re::Shader::Create(en::ShaderNames::k_cubeDepthShaderName));

				shadowStage->SetTextureTargetSet(curLight->GetShadowMap()->GetTextureTargetSet());

				shadowStage->SetStagePipelineState(shadowStageParams);

				// Cubemap shadow param block:
				CubemapShadowRenderParams cubemapShadowParams = GetCubemapShadowRenderParamsData(shadowCam);
				shared_ptr<re::ParameterBlock> cubemapShadowPB = re::ParameterBlock::Create(
					CubemapShadowRenderParams::s_shaderName,
					cubemapShadowParams,
					re::ParameterBlock::PBType::Mutable);

				shadowStage->AddPermanentParameterBlock(cubemapShadowPB);
				// TODO: The cubemap shadows param block should be created/maintained by the shadow map object, or the shadow camera

				pipeline.AppendRenderStage(shadowStage);
			}
			else
			{
				m_pointLightShadowStageCams.emplace_back(nullptr); // Ensure we stay in sync: 1 entry per stage
			}
		}
	}


	void ShadowsGraphicsSystem::PreRender()
	{
		CreateBatches();
		
		for (size_t i = 0; i < m_pointLightShadowStages.size(); i++)
		{
			Camera* shadowCam = m_pointLightShadowStageCams[i];
			if (shadowCam)
			{
				// Update the param block data:
				CubemapShadowRenderParams cubemapShadowParams = GetCubemapShadowRenderParamsData(shadowCam);

				shared_ptr<re::ParameterBlock> cubemapShadowPB = re::ParameterBlock::Create(
					CubemapShadowRenderParams::s_shaderName,
					cubemapShadowParams,
					re::ParameterBlock::PBType::SingleFrame);

				m_pointLightShadowStages[i]->AddSingleFrameParameterBlock(cubemapShadowPB);
			}
		}
	}


	void ShadowsGraphicsSystem::CreateBatches()
	{
		if (m_hasDirectionalLight)
		{
			m_directionalShadowStage->AddBatches(RenderManager::Get()->GetSceneBatches());
		}

		for (shared_ptr<RenderStage> pointShadowStage : m_pointLightShadowStages)
		{
			pointShadowStage->AddBatches(RenderManager::Get()->GetSceneBatches());
		}
	}


	std::shared_ptr<re::TextureTargetSet const> ShadowsGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_directionalShadowStage->GetTextureTargetSet();
	}
}