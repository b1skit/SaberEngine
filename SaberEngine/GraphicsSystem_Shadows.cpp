#include <memory>

#include "GraphicsSystem_Shadows.h"
#include "Light.h"
#include "ShadowMap.h"
#include "SceneManager.h"
#include "RenderManager.h"
#include "Config.h"

using en::SceneManager;
using en::Config;
using re::RenderManager;
using gr::Light;
using gr::RenderStage;
using gr::ShadowMap;
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
	ShadowsGraphicsSystem::ShadowsGraphicsSystem(std::string name) : GraphicsSystem(name), NamedObject(name),
		m_directionalShadowStage("Keylight shadow")
	{
	}


	void ShadowsGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		RenderStage::RenderStageParams shadowStageParams;
		shadowStageParams.m_targetClearMode = platform::Context::ClearTarget::Depth;
		
		// TODO: FaceCullingMode::Disabled is better for minimizing peter-panning, but we need backface culling if we
		// want to be able to place lights inside of geometry (eg. emissive spheres). For now, enable backface culling.
		// In future, we need to support tagging assets to not cast shadows
		shadowStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Back;

		shadowStageParams.m_srcBlendMode	= platform::Context::BlendMode::Disabled;
		shadowStageParams.m_dstBlendMode	= platform::Context::BlendMode::Disabled;
		shadowStageParams.m_depthTestMode	= platform::Context::DepthTestMode::Less;
		shadowStageParams.m_colorWriteMode	= 
		{ 
			platform::Context::ColorWriteMode::ChannelMode::Disabled,
			platform::Context::ColorWriteMode::ChannelMode::Disabled,
			platform::Context::ColorWriteMode::ChannelMode::Disabled,
			platform::Context::ColorWriteMode::ChannelMode::Disabled
		};

		// Directional light shadow:		
		shared_ptr<Light> directionalLight = SceneManager::GetSceneData()->GetKeyLight();
		if (directionalLight)
		{
			ShadowMap* const directionalShadow = directionalLight->GetShadowMap();
			if (directionalShadow)
			{
				m_directionalShadowStage.GetStageCamera() = directionalShadow->ShadowCamera();

				// Shader:
				m_directionalShadowStage.GetStageShader() = 
					make_shared<Shader>(Config::Get()->GetValue<string>("depthShaderName"));
				m_directionalShadowStage.GetStageShader()->Create();

				m_directionalShadowStage.GetTextureTargetSet() = directionalLight->GetShadowMap()->GetTextureTargetSet();
				// TODO: Target set should be a member of the stage, instead of the shadow map?

				m_directionalShadowStage.SetRenderStageParams(shadowStageParams);

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
				shadowStage->GetStageCamera() = shadowCam;
				
				// Shader:
				shadowStage->GetStageShader() = 
					make_shared<Shader>(Config::Get()->GetValue<string>("cubeDepthShaderName"));
				shadowStage->GetStageShader()->Create();

				shadowStage->GetTextureTargetSet() = curLight->GetShadowMap()->GetTextureTargetSet();

				shadowStage->SetRenderStageParams(shadowStageParams);

				// Cubemap shadow param block:
				CubemapShadowRenderParams cubemapShadowParams = GetCubemapShadowRenderParamsData(shadowCam);
				shared_ptr<re::ParameterBlock> cubemapShadowPB = re::ParameterBlock::Create(
					"CubemapShadowRenderParams",
					cubemapShadowParams,
					re::ParameterBlock::UpdateType::Mutable,
					re::ParameterBlock::Lifetime::Permanent);

				shadowStage->AddPermanentParameterBlock(cubemapShadowPB);
				// TODO: The cubemap shadows param block should be created/maintained by the shadow map object, or the shadow camera

				pipeline.AppendRenderStage(*shadowStage);
			}
		}
	}


	void ShadowsGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		m_directionalShadowStage.InitializeForNewFrame();

		for (shared_ptr<RenderStage> pointShadowStage : m_pointLightShadowStages)
		{
			pointShadowStage->InitializeForNewFrame();
		}

		CreateBatches();


		for (shared_ptr<RenderStage> pointShadowStage : m_pointLightShadowStages)
		{		
			Camera* shadowCam = pointShadowStage->GetStageCamera();

			// Update the param block data:
			shared_ptr<re::ParameterBlock> shadowParams =
				pointShadowStage->GetPermanentParameterBlock("CubemapShadowRenderParams");

			CubemapShadowRenderParams cubemapShadowParams = GetCubemapShadowRenderParamsData(shadowCam);

			shadowParams->SetData(cubemapShadowParams);
		}
	}


	void ShadowsGraphicsSystem::CreateBatches()
	{
		m_directionalShadowStage.AddBatches(RenderManager::Get()->GetSceneBatches());

		for (shared_ptr<RenderStage> pointShadowStage : m_pointLightShadowStages)
		{
			pointShadowStage->AddBatches(RenderManager::Get()->GetSceneBatches());
		}
	}
}