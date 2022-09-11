//#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "GraphicsSystem_DeferredLighting.h"
#include "SceneManager.h"
#include "CoreEngine.h"
#include "Light.h"
#include "ShadowMap.h"
#include "ImageBasedLight.h"
#include "RenderStage.h"
#include "GraphicsSystem_GBuffer.h"
#include "Mesh.h"


using gr::Light;
using gr::RenderStage;
using gr::Texture;
using gr::TextureTargetSet;
using gr::ShadowMap;
using en::CoreEngine;
using std::string;
using std::shared_ptr;
using std::make_shared;
using std::vector;
using glm::vec3;
using glm::vec4;


namespace gr
{
	DeferredLightingGraphicsSystem::DeferredLightingGraphicsSystem(string name) : GraphicsSystem(name),
		m_ambientStage("Ambient light stage"),
		m_keylightStage("Keylight stage"),
		m_pointlightStage("Pointlight stage")
	{
	}


	void DeferredLightingGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		shared_ptr<GBufferGraphicsSystem> gBufferGS = std::dynamic_pointer_cast<GBufferGraphicsSystem>(
			en::CoreEngine::GetRenderManager()->GetGraphicsSystem<GBufferGraphicsSystem>());
		SEAssert("GBuffer GS not found", gBufferGS != nullptr);
		
		// Create a shared lighting stage texture target:
		Texture::TextureParams lightTargetParams;
		lightTargetParams.m_width = en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes");
		lightTargetParams.m_height = en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes");
		lightTargetParams.m_faces = 1;
		lightTargetParams.m_texUse = Texture::TextureUse::ColorTarget;
		lightTargetParams.m_texDimension = Texture::TextureDimension::Texture2D;
		lightTargetParams.m_texFormat = Texture::TextureFormat::RGBA32F;
		lightTargetParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
		lightTargetParams.m_clearColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		lightTargetParams.m_texturePath = "DeferredLightTarget";

		std::shared_ptr<Texture> outputTexture = make_shared<Texture>(lightTargetParams);

		TextureTargetSet deferredLightingTargetSet("Deferred lighting target");
		deferredLightingTargetSet.ColorTarget(0) = outputTexture;
		deferredLightingTargetSet.DepthStencilTarget() = gBufferGS->GetFinalTextureTargetSet().DepthStencilTarget();
		deferredLightingTargetSet.CreateColorDepthStencilTargets();

		shared_ptr<Camera> deferredLightingCam = 
			en::CoreEngine::GetSceneManager()->GetMainCamera();

		
		// Set the target sets, even if the stages aren't actually used (to ensure they're still valid)
		m_ambientStage.GetTextureTargetSet() = deferredLightingTargetSet;
		m_keylightStage.GetTextureTargetSet() = deferredLightingTargetSet;
		m_pointlightStage.GetTextureTargetSet() = deferredLightingTargetSet;
	
		
		RenderStage::RenderStageParams ambientStageParams;
		ambientStageParams.m_targetClearMode	= platform::Context::ClearTarget::Color;
		ambientStageParams.m_faceCullingMode	= platform::Context::FaceCullingMode::Back; // Ambient and directional lights (currently) use back face culling
		ambientStageParams.m_srcBlendMode		= platform::Context::BlendMode::One; // All deferred lighting is additive
		ambientStageParams.m_dstBlendMode		= platform::Context::BlendMode::One;
		ambientStageParams.m_depthTestMode		= platform::Context::DepthTestMode::LEqual; // Ambient & directional
		ambientStageParams.m_depthWriteMode		= platform::Context::DepthWriteMode::Disabled;

		// Ambient light:
		shared_ptr<gr::ImageBasedLight> ambientLight = std::dynamic_pointer_cast<gr::ImageBasedLight>(
			en::CoreEngine::GetSceneManager()->GetAmbientLight());
		if (ambientLight)
		{
			m_ambientStage.GetStageShader() = ambientLight->GetDeferredLightShader();
			m_ambientStage.GetStageCamera() = deferredLightingCam;

			m_ambientStage.SetStageParams(ambientStageParams);

			m_ambientMesh.emplace_back(ambientLight->DeferredMesh());
			
			pipeline.AppendRenderStage(m_ambientStage);
		}
		
		
		// Key light:
		shared_ptr<Light> keyLight = en::CoreEngine::GetSceneManager()->GetKeyLight();
		
		RenderStage::RenderStageParams keylightStageParams(ambientStageParams);
		if (keyLight)
		{
			m_keylightStage.GetStageShader() = keyLight->GetDeferredLightShader();
			m_keylightStage.GetStageCamera() = deferredLightingCam;
			
			if (!ambientLight) // Don't clear after 1st light
			{
				keylightStageParams.m_targetClearMode = platform::Context::ClearTarget::Color;
			}
			else
			{
				keylightStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
			}
			m_keylightStage.SetStageParams(keylightStageParams);

			m_keylightMesh.emplace_back(keyLight->DeferredMesh());

			pipeline.AppendRenderStage(m_keylightStage);
		}

		// Point lights:
		vector<shared_ptr<Light>>& pointLights = en::CoreEngine::GetSceneManager()->GetPointLights();
		if (pointLights.size() > 0)
		{
			m_pointlightStage.GetStageCamera() = deferredLightingCam;

			RenderStage::RenderStageParams pointlightStageParams(keylightStageParams);

			// Pointlights only illuminate something if the sphere volume is behind it
			pointlightStageParams.m_depthTestMode = platform::Context::DepthTestMode::GEqual;

			if (!ambientLight && !keyLight) // Don't clear after 1st light
			{
				pointlightStageParams.m_targetClearMode = platform::Context::ClearTarget::Color;
			}
			else
			{
				pointlightStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
			}

			pointlightStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Front; // Pointlights cull front faces
			m_pointlightStage.SetStageParams(pointlightStageParams);

			// TEMP HAX: Store all pointlight meshes, so we can match them against entries in m_perMeshShaderUniforms
			//m_pointlightInstanceMesh.emplace_back(gr::meshfactory::CreateSphere(1.0f));
			for (shared_ptr<Light> const pointlight : pointLights)
			{
				m_pointlightInstanceMesh.emplace_back(pointlight->DeferredMesh());
				// Note: need to use the DeferredMesh here for now, so we get its transform
			}

			// All point lights use the same shader
			m_pointlightStage.GetStageShader() = pointLights[0]->GetDeferredLightShader(); 

			// Compute unchanging shader uniforms:
			const int xRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowXRes");
			const int yRes = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("windowYRes");
			const vec4 screenParams = vec4(xRes, yRes, 1.0f / xRes, 1.0f / yRes);
			m_pointlightStage.GetStageShader()->SetUniform(
				"screenParams",
				&screenParams.x,
				platform::Shader::UniformType::Vec4f,
				1);

			pipeline.AppendRenderStage(m_pointlightStage);
		}
	}


	void DeferredLightingGraphicsSystem::PreRender()
	{
		// Note: Culling is not (currently) supported. For now, we attempt to draw everything

		// TODO: Move some of these uniforms back to the Create() function, held in something that doesn't get cleared
		// every frame

		// Clear all stages for the new frame:
		m_ambientStage.InitializeForNewFrame();
		m_keylightStage.InitializeForNewFrame();
		m_pointlightStage.InitializeForNewFrame();
		// TODO: Is there some way to automate these calls so we don't need to remember them in every stage?

		// Light pointers:
		shared_ptr<gr::ImageBasedLight> const ambientLight = 
			std::dynamic_pointer_cast<gr::ImageBasedLight>(
				en::CoreEngine::GetSceneManager()->GetAmbientLight());
		shared_ptr<Light> const keyLight = en::CoreEngine::GetSceneManager()->GetKeyLight();
		vector<shared_ptr<Light>> const& pointLights = en::CoreEngine::GetSceneManager()->GetPointLights();

		// Re-set geometry for each stage:
		m_ambientStage.SetGeometryBatches(&m_ambientMesh);
		m_keylightStage.SetGeometryBatches(&m_keylightMesh);
		m_pointlightStage.SetGeometryBatches(&m_pointlightInstanceMesh);
		// TEMP HAX: Store all pointlight meshes, so we can match them against entries in m_perMeshShaderUniforms


		// Add GBuffer textures as stage inputs:		
		shared_ptr<GBufferGraphicsSystem> gBufferGS = std::dynamic_pointer_cast<GBufferGraphicsSystem>(
			en::CoreEngine::GetRenderManager()->GetGraphicsSystem<GBufferGraphicsSystem>());
		SEAssert("GBuffer GS not found", gBufferGS != nullptr);

		for (size_t i = 0; i < (GBufferGraphicsSystem::GBufferTexNames.size() - 1); i++) // -1, since we handle depth @end
		{
			if (GBufferGraphicsSystem::GBufferTexNames[i] == "GBufferEmissive")
			{
				// TEMP HAX!!!! Skip the emissive texture since we don't use it in the lighting shaders
				// -> Currently, we assert when trying to bind textures by name to a shader, if the name is not found...
				// TODO: Handle this more elegantly
				continue;
			}

			if (ambientLight)
			{
				m_ambientStage.SetTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[i],
					gBufferGS->GetFinalTextureTargetSet().ColorTarget(i).GetTexture(),
					Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear));
			}

			if (keyLight)
			{
				m_keylightStage.SetTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[i],
					gBufferGS->GetFinalTextureTargetSet().ColorTarget(i).GetTexture(),
					Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear));
			}

			if (pointLights.size() > 0)
			{
				m_pointlightStage.SetTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[i],
					gBufferGS->GetFinalTextureTargetSet().ColorTarget(i).GetTexture(),
					Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear));
			}
		}

		if (ambientLight)
		{
			// Add IBL texture inputs for ambient stage:
			m_ambientStage.SetTextureInput(
				"CubeMap0",
				ambientLight->GetIEMTexture(),
				Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear)
			);

			m_ambientStage.SetTextureInput(
				"CubeMap1",
				ambientLight->GetPMREMTexture(),
				Sampler::GetSampler(Sampler::SamplerType::WrapLinearMipMapLinearLinear)
			);

			m_ambientStage.SetTextureInput(
				"Tex7",
				ambientLight->GetBRDFIntegrationMap(),
				Sampler::GetSampler(Sampler::SamplerType::ClampNearestNearest)
			);
		}
		
		if (keyLight)
		{
			// Keylight shadowmap:		
			shared_ptr<ShadowMap> keyLightShadowMap = keyLight->GetShadowMap();
			SEAssert("Key light shadow map is null", keyLightShadowMap != nullptr);

			// Set the key light shadow map:
			shared_ptr<Texture> keylightDepthTex = 
				keyLightShadowMap->GetTextureTargetSet().DepthStencilTarget().GetTexture();
			m_keylightStage.SetTextureInput(
				"Depth0",
				keylightDepthTex,
				Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear));

			vec4 const keylightDepthTexelDims = keylightDepthTex->GetTexelDimenions();
			m_keylightStage.GetStageShader()->SetUniform(
				"texelSize", &keylightDepthTexelDims, platform::Shader::UniformType::Vec4f, 1);
			m_keylightStage.SetPerFrameShaderUniformByValue(
				"maxShadowBias", keyLightShadowMap->MaxShadowBias(), platform::Shader::UniformType::Float, 1);
			m_keylightStage.SetPerFrameShaderUniformByValue(
				"minShadowBias", keyLightShadowMap->MinShadowBias(), platform::Shader::UniformType::Float, 1);

			// Keylight shadow camera:
			shared_ptr<Camera> const keyLightShadowCam = keyLightShadowMap->ShadowCamera();
			m_keylightStage.SetPerFrameShaderUniformByValue( // GetViewProjectionMatrix() returns by value; must cache it for later
				"shadowCam_vp", keyLightShadowCam->GetViewProjectionMatrix(), platform::Shader::UniformType::Matrix4x4f, 1);
			m_keylightStage.SetPerFrameShaderUniformByValue(
				"shadowCam_near", keyLightShadowCam->Near(), platform::Shader::UniformType::Float, 1);
			m_keylightStage.SetPerFrameShaderUniformByValue(
				"shadowCam_far", keyLightShadowCam->Far(), platform::Shader::UniformType::Float, 1);

			// Keylight properties:
			m_keylightStage.SetPerFrameShaderUniformByPtr( // GetColor() returns by reference
				"lightColor", &keyLight->GetColor().r, platform::Shader::UniformType::Vec3f, 1);

			m_keylightStage.SetPerFrameShaderUniformByPtr( // Returns by reference
				"keylightWorldDir", &keyLight->GetTransform().Forward().x, platform::Shader::UniformType::Vec3f, 1);

			m_keylightStage.SetPerFrameShaderUniformByValue(
				"keylightViewDir",
				glm::normalize(m_keylightStage.GetStageCamera()->GetViewMatrix() * vec4(keyLight->GetTransform().Forward(), 0.0f)),
				platform::Shader::UniformType::Vec3f,
				1);
		}

		if (pointLights.size() > 0)
		{
			// TODO: Support instancing. For now, just pack a vector with vectors of per-mesh parameters
			for (size_t lightIdx = 0; lightIdx < pointLights.size(); lightIdx++)
			{
				m_pointlightStage.SetPerMeshPerFrameShaderUniformByPtr(
					lightIdx,
					"lightColor",
					&pointLights[lightIdx]->GetColor().r, // Returns reference
					platform::Shader::UniformType::Vec3f,
					1);

				m_pointlightStage.SetPerMeshPerFrameShaderUniformByPtr(
					lightIdx,
					"lightWorldPos",
					&pointLights[lightIdx]->GetTransform().GetWorldPosition().x, // Returns reference
					platform::Shader::UniformType::Vec3f,
					1);

				std::shared_ptr<ShadowMap> activeShadowMap = pointLights[lightIdx]->GetShadowMap();
				if (activeShadowMap != nullptr)
				{
					std::shared_ptr<Camera> shadowCam = activeShadowMap->ShadowCamera();
					if (shadowCam != nullptr)
					{
						m_pointlightStage.SetPerMeshPerFrameShaderUniformByValue(
							lightIdx,
							"shadowCam_vp",
							shadowCam->GetViewProjectionMatrix(), //returns value
							platform::Shader::UniformType::Matrix4x4f,
							1);

						m_pointlightStage.SetPerMeshPerFrameShaderUniformByValue(
							lightIdx,
							"maxShadowBias",
							activeShadowMap->MaxShadowBias(),
							platform::Shader::UniformType::Float,
							1);

						m_pointlightStage.SetPerMeshPerFrameShaderUniformByValue(
							lightIdx,
							"minShadowBias",
							activeShadowMap->MinShadowBias(),
							platform::Shader::UniformType::Float,
							1);

						m_pointlightStage.SetPerMeshPerFrameShaderUniformByValue(
							lightIdx,
							"shadowCam_near",
							shadowCam->Near(),
							platform::Shader::UniformType::Float,
							1);

						m_pointlightStage.SetPerMeshPerFrameShaderUniformByValue(
							lightIdx,
							"shadowCam_far",
							shadowCam->Far(),
							platform::Shader::UniformType::Float,
							1);

						// Manually set depth textures and samplers...
						std::shared_ptr<gr::Texture> depthTexture =
							activeShadowMap->GetTextureTargetSet().DepthStencilTarget().GetTexture();
						m_pointlightStage.SetPerMeshPerFrameShaderUniformByPtr(
							lightIdx,
							"CubeMap0",
							depthTexture.get(),
							platform::Shader::UniformType::Texture,
							1);
						m_pointlightStage.SetPerMeshPerFrameShaderUniformByPtr(
							lightIdx,
							"CubeMap0",
							Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear).get(),
							platform::Shader::UniformType::Sampler,
							1);

						m_pointlightStage.SetPerMeshPerFrameShaderUniformByValue(
							lightIdx,
							"texelSize",
							depthTexture->GetTexelDimenions(),
							platform::Shader::UniformType::Vec4f,
							1);
					}
				}
			}
		} // pointlights

	}
}