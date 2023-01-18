// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "GraphicsSystem_Shadows.h"
#include "Light.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "ShadowMap.h"


namespace
{
	using glm::mat4;
	using glm::vec2;
	using glm::vec3;


	struct CubemapShadowRenderParams
	{
		glm::mat4 g_cubemapShadowCam_VP[6];

		glm::vec2 g_cubemapShadowCamNearFar; // .xy = near, far
		const glm::vec2 padding0 = { 0.f, 0.f };

		glm::vec3 g_cubemapLightWorldPos;
		const float padding1 = 0.f;
	};


	CubemapShadowRenderParams GetCubemapShadowRenderParamsData(gr::Camera* shadowCam)
	{
		CubemapShadowRenderParams cubemapShadowParams;
		memcpy(&cubemapShadowParams.g_cubemapShadowCam_VP[0][0].x,
			shadowCam->GetCubeViewProjectionMatrix().data(),
			6 * sizeof(mat4));
		cubemapShadowParams.g_cubemapShadowCamNearFar = shadowCam->NearFar();
		cubemapShadowParams.g_cubemapLightWorldPos = shadowCam->GetTransform()->GetGlobalPosition();

		return cubemapShadowParams;
	}
}


namespace gr
{
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


	ShadowsGraphicsSystem::ShadowsGraphicsSystem(std::string name) 
		: GraphicsSystem(name), NamedObject(name)
		, m_directionalShadowStage("Keylight shadow")
		, m_hasDirectionalLight(false)
	{
	}


	void ShadowsGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		RenderStage::PipelineStateParams shadowStageParams;
		shadowStageParams.m_targetClearMode = re::Context::ClearTarget::Depth;
		
		// TODO: FaceCullingMode::Disabled is better for minimizing peter-panning, but we need backface culling if we
		// want to be able to place lights inside of geometry (eg. emissive spheres). For now, enable backface culling.
		// In future, we need to support tagging assets to not cast shadows
		shadowStageParams.m_faceCullingMode = re::Context::FaceCullingMode::Back;

		shadowStageParams.m_srcBlendMode	= re::Context::BlendMode::Disabled;
		shadowStageParams.m_dstBlendMode	= re::Context::BlendMode::Disabled;
		shadowStageParams.m_depthTestMode	= re::Context::DepthTestMode::Less;
		shadowStageParams.m_colorWriteMode	= 
		{ 
			re::Context::ColorWriteMode::ChannelMode::Disabled,
			re::Context::ColorWriteMode::ChannelMode::Disabled,
			re::Context::ColorWriteMode::ChannelMode::Disabled,
			re::Context::ColorWriteMode::ChannelMode::Disabled
		};

		// Directional light shadow:		
		shared_ptr<Light> directionalLight = SceneManager::GetSceneData()->GetKeyLight();
		if (directionalLight)
		{
			m_hasDirectionalLight = true;

			ShadowMap* const directionalShadow = directionalLight->GetShadowMap();
			if (directionalShadow)
			{
				m_directionalShadowStage.SetStageCamera(directionalShadow->ShadowCamera());

				// Shader:
				m_directionalShadowStage.GetStageShader() = 
					make_shared<Shader>(Config::Get()->GetValue<string>("depthShaderName"));

				m_directionalShadowStage.SetTextureTargetSet(directionalLight->GetShadowMap()->GetTextureTargetSet());
				// TODO: Target set should be a member of the stage, instead of the shadow map?
				// -> HARD: The stages are already created, we don't know what lights are associated with each stage

				m_directionalShadowStage.SetStagePipelineStateParams(shadowStageParams);

				pipeline.AppendRenderStage(m_directionalShadowStage);
			}
		}
		
		// Point light shadows:
		vector<shared_ptr<Light>> const& deferredLights = SceneManager::GetSceneData()->GetPointLights();
		for (shared_ptr<Light> curLight : deferredLights)
		{
			m_pointLightShadowStages.emplace_back(make_shared<RenderStage>(curLight->GetName() + " shadow"));

			RenderStage* shadowStage = m_pointLightShadowStages.back().get();
			
			ShadowMap* const lightShadow = curLight->GetShadowMap();
			if (lightShadow)
			{
				Camera* const shadowCam = lightShadow->ShadowCamera();
				shadowStage->SetStageCamera(shadowCam);
				
				// Shader:
				shadowStage->GetStageShader() = 
					make_shared<Shader>(Config::Get()->GetValue<string>("cubeDepthShaderName"));

				shadowStage->SetTextureTargetSet(curLight->GetShadowMap()->GetTextureTargetSet());

				shadowStage->SetStagePipelineStateParams(shadowStageParams);

				// Cubemap shadow param block:
				CubemapShadowRenderParams cubemapShadowParams = GetCubemapShadowRenderParamsData(shadowCam);
				shared_ptr<re::ParameterBlock> cubemapShadowPB = re::ParameterBlock::Create(
					"CubemapShadowRenderParams",
					cubemapShadowParams,
					re::ParameterBlock::PBType::Mutable);

				shadowStage->AddPermanentParameterBlock(cubemapShadowPB);
				// TODO: The cubemap shadows param block should be created/maintained by the shadow map object, or the shadow camera

				pipeline.AppendRenderStage(*shadowStage);
			}
		}
	}


	void ShadowsGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();

		for (shared_ptr<RenderStage> pointShadowStage : m_pointLightShadowStages)
		{		
			Camera* shadowCam = pointShadowStage->GetStageCamera();

			// Update the param block data:
			CubemapShadowRenderParams cubemapShadowParams = GetCubemapShadowRenderParamsData(shadowCam);

			shared_ptr<re::ParameterBlock> cubemapShadowPB = re::ParameterBlock::Create(
				"CubemapShadowRenderParams", 
				cubemapShadowParams,
				re::ParameterBlock::PBType::SingleFrame);

			pointShadowStage->AddSingleFrameParameterBlock(cubemapShadowPB);
		}
	}


	void ShadowsGraphicsSystem::CreateBatches()
	{
		if (m_hasDirectionalLight)
		{
			m_directionalShadowStage.AddBatches(RenderManager::Get()->GetSceneBatches());
		}

		for (shared_ptr<RenderStage> pointShadowStage : m_pointLightShadowStages)
		{
			pointShadowStage->AddBatches(RenderManager::Get()->GetSceneBatches());
		}
	}


	std::shared_ptr<re::TextureTargetSet> ShadowsGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_directionalShadowStage.GetTextureTargetSet();
	}
}