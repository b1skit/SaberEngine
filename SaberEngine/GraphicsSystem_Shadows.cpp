#include "GraphicsSystem_Shadows.h"
#include "Light.h"
#include "ShadowMap.h"
#include "CoreEngine.h"

using en::CoreEngine;
using gr::Light;
using gr::RenderStage;
using gr::ShadowMap;
using std::vector;
using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::make_unique;
using glm::mat4;
using glm::vec3;


namespace gr
{
	ShadowsGraphicsSystem::ShadowsGraphicsSystem(std::string name) : GraphicsSystem(name),
		m_directionalShadowStage("Keylight shadow")
	{
	}


	void ShadowsGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		RenderStage::RenderStageParams shadowStageParams;
		shadowStageParams.m_targetClearMode = platform::Context::ClearTarget::Depth;
		shadowStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Disabled; // Minimize peter-panning
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
		shared_ptr<Light> directionalLight = CoreEngine::GetSceneManager()->GetSceneData()->GetKeyLight();
		if (directionalLight)
		{
			shared_ptr<ShadowMap> directionalShadow = directionalLight->GetShadowMap();
			if (directionalShadow)
			{
				m_directionalShadowStage.GetStageCamera() = directionalShadow->ShadowCamera();
				m_directionalShadowStage.GetStageShader() = directionalShadow->ShadowCamera()->GetRenderShader();

				m_directionalShadowStage.GetTextureTargetSet() = directionalLight->GetShadowMap()->GetTextureTargetSet();
				// TODO: Target set should be a member of the stage, instead of the shadow map?


				m_directionalShadowStage.SetStageParams(shadowStageParams);

				pipeline.AppendRenderStage(m_directionalShadowStage);
			}
		}
		
		// Point light shadows:
		vector<shared_ptr<Light>> const& deferredLights = CoreEngine::GetSceneManager()->GetSceneData()->GetPointLights();
		for (shared_ptr<Light> curLight : deferredLights)
		{
			m_pointLightShadowStages.emplace_back(make_shared<RenderStage>(curLight->GetName() + " shadow"));

			RenderStage* shadowStage = m_pointLightShadowStages.back().get();
			
			shared_ptr<ShadowMap> const lightShadow = curLight->GetShadowMap();
			if (lightShadow)
			{
				std::shared_ptr<Camera> const shadowCam = lightShadow->ShadowCamera();
				shadowStage->GetStageCamera() = shadowCam;
				shadowStage->GetStageShader() = shadowCam->GetRenderShader();
				shadowStage->GetTextureTargetSet() = curLight->GetShadowMap()->GetTextureTargetSet();

				shadowStage->SetStageParams(shadowStageParams);

				pipeline.AppendRenderStage(*shadowStage);
			}
		}
	}


	void ShadowsGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		m_directionalShadowStage.InitializeForNewFrame();
		m_directionalShadowStage.SetGeometryBatches(&CoreEngine::GetSceneManager()->GetSceneData()->GetMeshes());


		for (shared_ptr<RenderStage> pointShadowStage : m_pointLightShadowStages)
		{
			pointShadowStage->InitializeForNewFrame();
			pointShadowStage->SetGeometryBatches(&CoreEngine::GetSceneManager()->GetSceneData()->GetMeshes());

			shared_ptr<Camera> pointShadowCam = pointShadowStage->GetStageCamera();

			pointShadowStage->SetPerFrameShaderUniformByValue(
				"shadowCamCubeMap_vp", pointShadowCam->GetCubeViewProjectionMatrix(), platform::Shader::UniformType::Matrix4x4f, 6);
			pointShadowStage->SetPerFrameShaderUniformByValue(
				"lightWorldPos", (*(pointShadowCam->GetTransform())).GetWorldPosition(), platform::Shader::UniformType::Vec3f, 1);
			pointShadowStage->SetPerFrameShaderUniformByValue(
				"shadowCam_near", pointShadowCam->Near(), platform::Shader::UniformType::Float, 1);
			pointShadowStage->SetPerFrameShaderUniformByValue(
				"shadowCam_far", pointShadowCam->Far(), platform::Shader::UniformType::Float, 1);
		}
	}
}