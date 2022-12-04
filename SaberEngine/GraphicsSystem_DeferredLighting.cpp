#include <glm/glm.hpp>

#include "GraphicsSystem_DeferredLighting.h"
#include "SceneManager.h"
#include "Config.h"
#include "RenderManager.h"
#include "Light.h"
#include "ShadowMap.h"
#include "RenderStage.h"
#include "GraphicsSystem_GBuffer.h"
#include "MeshPrimitive.h"
#include "Batch.h"
#include "ParameterBlock.h"
#include "SceneManager.h"

using gr::Light;
using re::RenderStage;
using gr::Texture;
using re::TextureTargetSet;
using gr::ShadowMap;
using re::RenderManager;
using re::ParameterBlock;
using re::Batch;
using en::Config;
using en::SceneManager;
using std::string;
using std::shared_ptr;
using std::make_shared;
using std::vector;
using std::to_string;
using glm::vec3;
using glm::vec4;
using glm::mat4;


namespace
{
	constexpr uint32_t k_generatedAmbientIBLTexRes = 512; // TODO: Make this user-controllable via the config


	struct AmbientLightParams
	{
		uint32_t g_maxPMREMMip;
	};


	AmbientLightParams GetAmbientLightParamData()
	{
		AmbientLightParams ambientLightParams;
		ambientLightParams.g_maxPMREMMip = (uint32_t)glm::log2((float)k_generatedAmbientIBLTexRes);

		return ambientLightParams;
	}

	
	struct LightParams
	{
		glm::vec3 g_lightColorIntensity;
		const float padding0 = 0.f;

		// Directional lights: Normalized, world-space dir pointing towards source (ie. parallel)
		glm::vec3 g_lightWorldPos;
		const float padding1 = 0.f;

		glm::vec4 g_shadowMapTexelSize;	// .xyzw = width, height, 1/width, 1/height

		glm::vec2 g_shadowCamNearFar;
		glm::vec2 g_shadowBiasMinMax; // .xy = min, max shadow bias

		glm::mat4 g_shadowCam_VP;
	};


	LightParams GetLightParamData(shared_ptr<Light> const light)
	{
		LightParams lightParams;
		memset(&lightParams, 0, sizeof(LightParams)); // Ensure unused elements are zeroed

		lightParams.g_lightColorIntensity = light->GetColor();

		// Type-specific params:
		switch (light->Type())
		{
		case gr::Light::LightType::Directional:
		{
			lightParams.g_lightWorldPos = light->GetTransform()->GetGlobalForward(); // WorldPos == Light dir
		}
		break;
		case gr::Light::LightType::Point:
		{
			lightParams.g_lightWorldPos = light->GetTransform()->GetGlobalPosition();
		}
		break;
		default:
			SEAssertF("Light type does not use this param block");
		}
		
		gr::ShadowMap* const shadowMap = light->GetShadowMap();
		if (shadowMap)
		{
			lightParams.g_shadowMapTexelSize =
				shadowMap->GetTextureTargetSet().DepthStencilTarget().GetTexture()->GetTextureDimenions();

			lightParams.g_shadowBiasMinMax = shadowMap->MinMaxShadowBias();

			gr::Camera* const shadowCam = shadowMap->ShadowCamera();
			lightParams.g_shadowCamNearFar = shadowCam->NearFar();

			// Type-specific shadow params:
			switch (light->Type())
			{
			case gr::Light::LightType::Directional:
			{
				lightParams.g_shadowCam_VP = shadowCam->GetViewProjectionMatrix();
			}
			break;
			case gr::Light::LightType::Point:
			{
				lightParams.g_shadowCam_VP = glm::mat4(0.0f); // Unused by point light cube shadow maps
			}
			break;
			default:
				SEAssertF("Light shadow type does not use this param block");
			}
		}
		return lightParams;
	}
}


namespace gr
{
	DeferredLightingGraphicsSystem::DeferredLightingGraphicsSystem(string name) : GraphicsSystem(name), NamedObject(name),
		m_ambientStage("Ambient light stage"),
		m_keylightStage("Keylight stage"),
		m_pointlightStage("Pointlight stage"),
		m_BRDF_integrationMap(nullptr)
	{
		// Create a fullscreen quad, for reuse when building batches:
		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Near);

		// Cube mesh, for rendering of IBL cubemaps
		m_cubeMeshPrimitive = meshfactory::CreateCube();
	}


	void DeferredLightingGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		shared_ptr<GBufferGraphicsSystem> gBufferGS = std::dynamic_pointer_cast<GBufferGraphicsSystem>(
			RenderManager::Get()->GetGraphicsSystem<GBufferGraphicsSystem>());
		SEAssert("GBuffer GS not found", gBufferGS != nullptr);
		
		// Create a shared lighting stage texture target:
		Texture::TextureParams lightTargetParams;
		lightTargetParams.m_width = Config::Get()->GetValue<int>("windowXRes");
		lightTargetParams.m_height = Config::Get()->GetValue<int>("windowYRes");
		lightTargetParams.m_faces = 1;
		lightTargetParams.m_usage = Texture::Usage::ColorTarget;
		lightTargetParams.m_dimension = Texture::Dimension::Texture2D;
		lightTargetParams.m_format = Texture::Format::RGBA32F;
		lightTargetParams.m_colorSpace = Texture::ColorSpace::Linear;
		lightTargetParams.m_clearColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);

		std::shared_ptr<Texture> outputTexture = make_shared<Texture>("DeferredLightTarget", lightTargetParams);

		TextureTargetSet deferredLightingTargetSet("Deferred lighting target");
		deferredLightingTargetSet.ColorTarget(0) = outputTexture;
		deferredLightingTargetSet.DepthStencilTarget() = gBufferGS->GetFinalTextureTargetSet().DepthStencilTarget();
		deferredLightingTargetSet.CreateColorDepthStencilTargets();

		Camera* deferredLightingCam = SceneManager::GetSceneData()->GetMainCamera().get();

		
		// Set the target sets, even if the stages aren't actually used (to ensure they're still valid)
		m_ambientStage.GetTextureTargetSet() = deferredLightingTargetSet;
		m_keylightStage.GetTextureTargetSet() = deferredLightingTargetSet;
		m_pointlightStage.GetTextureTargetSet() = deferredLightingTargetSet;
	
		
		RenderStage::PipelineStateParams ambientStageParams;
		ambientStageParams.m_targetClearMode	= platform::Context::ClearTarget::Color;
		ambientStageParams.m_faceCullingMode	= platform::Context::FaceCullingMode::Back; // Ambient and directional lights (currently) use back face culling
		ambientStageParams.m_srcBlendMode		= platform::Context::BlendMode::One; // All deferred lighting is additive
		ambientStageParams.m_dstBlendMode		= platform::Context::BlendMode::One;
		ambientStageParams.m_depthTestMode		= platform::Context::DepthTestMode::LEqual; // Ambient & directional
		ambientStageParams.m_depthWriteMode		= platform::Context::DepthWriteMode::Disabled;


		// Ambient lights are not supported by GLTF 2.0; Instead, we just check for a \IBL\ibl.hdr file.
		// Attempt to load the source IBL image (gets a pink error image if it fails)
		const string sceneIBLPath = Config::Get()->GetValue<string>("sceneIBLPath");
		shared_ptr<Texture> iblTexture = SceneManager::GetSceneData()->GetLoadTextureByPath({ sceneIBLPath }, false);
		if (!iblTexture)
		{
			const string defaultIBLPath = Config::Get()->GetValue<string>("defaultIBLPath");
			iblTexture = SceneManager::GetSceneData()->GetLoadTextureByPath({ defaultIBLPath }, true);
		}
		Texture::TextureParams iblParams = iblTexture->GetTextureParams();
		iblParams.m_colorSpace = Texture::ColorSpace::Linear;
		iblTexture->SetTextureParams(iblParams);


		// 1st frame: Generate the pre-integrated BRDF LUT via a single-frame render stage:
		{
			RenderStage brdfStage("BRDF pre-integration stage");

			brdfStage.GetStageShader() = make_shared<Shader>(
				Config::Get()->GetValue<string>("BRDFIntegrationMapShaderName"));

			// Create a render target texture:			
			Texture::TextureParams brdfParams;
			brdfParams.m_width = k_generatedAmbientIBLTexRes;
			brdfParams.m_height = k_generatedAmbientIBLTexRes;
			brdfParams.m_faces = 1;
			brdfParams.m_usage = Texture::Usage::ColorTarget;
			brdfParams.m_dimension = Texture::Dimension::Texture2D;
			brdfParams.m_format = Texture::Format::RG16F; // Epic recommends 2 channel, 16-bit floats
			brdfParams.m_colorSpace = Texture::ColorSpace::Linear;
			brdfParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
			brdfParams.m_useMIPs = false;

			m_BRDF_integrationMap = std::make_shared<gr::Texture>("BRDFIntegrationMap", brdfParams);

			brdfStage.GetTextureTargetSet().ColorTarget(0) = m_BRDF_integrationMap;
			brdfStage.GetTextureTargetSet().Viewport() = 
				re::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes);
			brdfStage.GetTextureTargetSet().CreateColorTargets();

			// Stage params:
			RenderStage::PipelineStateParams brdfStageParams;
			brdfStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
			brdfStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Disabled;
			brdfStageParams.m_srcBlendMode = platform::Context::BlendMode::One;
			brdfStageParams.m_dstBlendMode = platform::Context::BlendMode::Zero;
			brdfStageParams.m_depthTestMode = platform::Context::DepthTestMode::Always;
			brdfStageParams.m_depthWriteMode = platform::Context::DepthWriteMode::Disabled;

			brdfStage.SetStagePipelineStateParams(brdfStageParams);

			Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr, nullptr);
			brdfStage.AddBatch(fullscreenQuadBatch);

			pipeline.AppendSingleFrameRenderStage(brdfStage);
		}


		// Common IBL cubemap params:
		Texture::TextureParams cubeParams;
		cubeParams.m_width = k_generatedAmbientIBLTexRes;
		cubeParams.m_height = k_generatedAmbientIBLTexRes;
		cubeParams.m_faces = 6;
		cubeParams.m_usage = Texture::Usage::ColorTarget;
		cubeParams.m_dimension = Texture::Dimension::TextureCubeMap;
		cubeParams.m_format = Texture::Format::RGB16F;
		cubeParams.m_colorSpace = Texture::ColorSpace::Linear;

		// Common IBL texture generation stage params:
		RenderStage::PipelineStateParams iblStageParams;
		iblStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
		iblStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Disabled;
		iblStageParams.m_srcBlendMode = platform::Context::BlendMode::One;
		iblStageParams.m_dstBlendMode = platform::Context::BlendMode::Zero;
		iblStageParams.m_depthTestMode = platform::Context::DepthTestMode::Always;
		iblStageParams.m_depthWriteMode = platform::Context::DepthWriteMode::Disabled;

		const mat4 cubeProjectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		const mat4 cubemapViews[] =
		{
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  1.0f,  0.0f), vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, -1.0f,  0.0f), vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  0.0f,  1.0f), vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  0.0f, -1.0f), vec3(0.0f, -1.0f,  0.0f))
		};

		// Common cubemap camera rendering params; Just need to update g_view for each face/stage
		Camera::CameraParams cubemapCamParams;
		cubemapCamParams.g_projection = cubeProjectionMatrix;
		cubemapCamParams.g_viewProjection = glm::mat4(1.f); // Identity; unused
		cubemapCamParams.g_invViewProjection = glm::mat4(1.f); // Identity; unused
		cubemapCamParams.g_cameraWPos = vec3(0.f, 0.f, 0.f); // Unused

		// Create a cube mesh batch, for reuse during the initial frame IBL rendering:
		Batch cubeMeshBatch = Batch(m_cubeMeshPrimitive.get(), nullptr, nullptr);

		const string equilinearToCubemapShaderName =
			Config::Get()->GetValue<string>("equilinearToCubemapBlitShaderName");

		// TODO: We should use equirectangular images, instead of bothering to convert to cubemaps for IEM/PMREM


		// 1st frame: Generate an IEM (Irradiance Environment Map) cubemap texture for diffuse irradiance
		{
			shared_ptr<Shader> iemShader = make_shared<gr::Shader>(equilinearToCubemapShaderName);
			iemShader->ShaderKeywords().emplace_back("BLIT_IEM");

			// IEM-specific texture params:
			cubeParams.m_useMIPs = false;
			m_IEMTex = make_shared<Texture>("IEMTexture", cubeParams);

			for (uint32_t face = 0; face < 6; face++)
			{
				RenderStage iemStage("IEM generation: Face " + to_string(face + 1) + "/6");

				iemStage.GetStageShader() = iemShader;
				iemStage.SetTextureInput(
					"MatAlbedo",
					iblTexture,
					gr::Sampler::GetSampler(gr::Sampler::WrapAndFilterMode::ClampLinearMipMapLinearLinear));

				const int numSamples = Config::Get()->GetValue<int>("numIEMSamples");
				iemStage.SetPerFrameShaderUniform("numSamples", numSamples, gr::Shader::UniformType::Int, 1);
				
				// Construct a camera param block to draw into our cubemap rendering targets:
				cubemapCamParams.g_view = cubemapViews[face];
				shared_ptr<re::ParameterBlock> pb = re::ParameterBlock::Create(
					"CameraParams",
					cubemapCamParams,
					re::ParameterBlock::UpdateType::Immutable,
					re::ParameterBlock::Lifetime::SingleFrame);
				iemStage.AddPermanentParameterBlock(pb);

				iemStage.GetTextureTargetSet().ColorTarget(0) = m_IEMTex;
				iemStage.GetTextureTargetSet().Viewport() = 
					re::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes);
				iemStage.GetTextureTargetSet().CreateColorTargets();

				iblStageParams.m_textureTargetSetConfig.m_targetFace = face;
				iblStageParams.m_textureTargetSetConfig.m_targetMip = 0;
				iemStage.SetStagePipelineStateParams(iblStageParams);

				iemStage.AddBatch(cubeMeshBatch);

				pipeline.AppendSingleFrameRenderStage(iemStage);
			}
		}

		// 1st frame: Generate PMREM (Pre-filtered Mip-mapped Radiance Environment Map) cubemap for specular reflections
		{
			shared_ptr<Shader> pmremShader = make_shared<gr::Shader>(equilinearToCubemapShaderName);
			pmremShader->ShaderKeywords().emplace_back("BLIT_PMREM");

			// PMREM-specific texture params:
			cubeParams.m_useMIPs = true;
			m_PMREMTex = make_shared<Texture>("PMREMTexture", cubeParams);

			TextureTargetSet pmremTargetSet("PMREM texture targets");
			pmremTargetSet.ColorTarget(0) = m_PMREMTex;
			pmremTargetSet.Viewport() = re::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes);
			pmremTargetSet.CreateColorTargets();

			const uint32_t numMipLevels = m_PMREMTex->GetNumMips(); // # of mips we need to render

			for (uint32_t currentMipLevel = 0; currentMipLevel < numMipLevels; currentMipLevel++)
			{
				for (uint32_t face = 0; face < 6; face++)
				{
					RenderStage pmremStage(
						"PMREM generation: Face " + to_string(face + 1) + "/6, MIP " +
						to_string(currentMipLevel + 1) + "/" + to_string(numMipLevels));

					pmremStage.GetStageShader() = pmremShader;
					pmremStage.SetTextureInput(
						"MatAlbedo",
						iblTexture,
						gr::Sampler::GetSampler(gr::Sampler::WrapAndFilterMode::ClampLinearMipMapLinearLinear));

					const int numSamples = Config::Get()->GetValue<int>("numPMREMSamples");
					pmremStage.SetPerFrameShaderUniform("numSamples", numSamples, gr::Shader::UniformType::Int, 1);
					
					// Construct a camera param block to draw into our cubemap rendering targets:
					cubemapCamParams.g_view = cubemapViews[face];
					shared_ptr<re::ParameterBlock> pb = re::ParameterBlock::Create(
						"CameraParams",
						cubemapCamParams,
						re::ParameterBlock::UpdateType::Immutable,
						re::ParameterBlock::Lifetime::SingleFrame);
					pmremStage.AddPermanentParameterBlock(pb);

					const float roughness = (float)currentMipLevel / (float)(numMipLevels - 1);
					pmremStage.SetPerFrameShaderUniform("roughness", roughness, gr::Shader::UniformType::Float, 1);

					pmremStage.GetTextureTargetSet() = pmremTargetSet;

					iblStageParams.m_textureTargetSetConfig.m_targetFace = face;
					iblStageParams.m_textureTargetSetConfig.m_targetMip = currentMipLevel;
					pmremStage.SetStagePipelineStateParams(iblStageParams);

					pmremStage.AddBatch(cubeMeshBatch);

					pipeline.AppendSingleFrameRenderStage(pmremStage);
				}
			}
		}

		
		// Ambient light stage:
		m_ambientStage.GetStageShader() = make_shared<Shader>(
			Config::Get()->GetValue<string>("deferredAmbientLightShaderName"));
		m_ambientStage.GetStageShader()->ShaderKeywords().emplace_back("AMBIENT_IBL");

		m_ambientStage.GetStageCamera() = deferredLightingCam;
		m_ambientStage.SetStagePipelineStateParams(ambientStageParams);

		// Ambient parameters:		
		AmbientLightParams ambientLightParams = GetAmbientLightParamData();
		std::shared_ptr<re::ParameterBlock> ambientLightPB = re::ParameterBlock::Create(
			"AmbientLightParams",
			ambientLightParams,
			re::ParameterBlock::UpdateType::Immutable,
			re::ParameterBlock::Lifetime::Permanent);

		m_ambientStage.AddPermanentParameterBlock(ambientLightPB);

		// If we made it this far, append the ambient stage:
		pipeline.AppendRenderStage(m_ambientStage);
		

		// Key light stage:
		shared_ptr<Light> keyLight = SceneManager::GetSceneData()->GetKeyLight();

		RenderStage::PipelineStateParams keylightStageParams(ambientStageParams);
		if (keyLight)
		{
			if (!AmbientIsValid()) // Don't clear after 1st light
			{
				keylightStageParams.m_targetClearMode = platform::Context::ClearTarget::Color;
			}
			else
			{
				keylightStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
			}
			m_keylightStage.SetStagePipelineStateParams(keylightStageParams);

			m_keylightStage.GetStageShader() = make_shared<Shader>(
				Config::Get()->GetValue<string>("deferredKeylightShaderName"));

			m_keylightStage.GetStageCamera() = deferredLightingCam;

			pipeline.AppendRenderStage(m_keylightStage);
		}


		// Point light stage:
		vector<shared_ptr<Light>> const& pointLights = SceneManager::GetSceneData()->GetPointLights();
		if (pointLights.size() > 0)
		{
			m_pointlightStage.GetStageCamera() = deferredLightingCam;

			RenderStage::PipelineStateParams pointlightStageParams(keylightStageParams);

			if (!keyLight && !AmbientIsValid())
			{
				keylightStageParams.m_targetClearMode = platform::Context::ClearTarget::Color;
			}

			// Pointlights only illuminate something if the sphere volume is behind it
			pointlightStageParams.m_depthTestMode = platform::Context::DepthTestMode::GEqual;

			if (!iblTexture && !keyLight) // Don't clear after 1st light
			{
				pointlightStageParams.m_targetClearMode = platform::Context::ClearTarget::Color;
			}
			else
			{
				pointlightStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
			}

			pointlightStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Front; // Cull front faces of light volumes
			m_pointlightStage.SetStagePipelineStateParams(pointlightStageParams);

			m_pointlightStage.GetStageShader() = make_shared<Shader>(
				Config::Get()->GetValue<string>("deferredPointLightShaderName"));

			pipeline.AppendRenderStage(m_pointlightStage);

			// Create a sphere mesh for each pointlights:
			for (shared_ptr<Light> pointlight : pointLights)
			{
				m_sphereMeshes.emplace_back(
					std::make_shared<gr::Mesh>(pointlight->GetTransform(), meshfactory::CreateSphere(1.0f)));
			}
		}
	}


	void DeferredLightingGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		// Note: Culling is not (currently) supported. For now, we attempt to draw everything

		// Clear all stages for the new frame:
		m_ambientStage.InitializeForNewFrame();
		m_keylightStage.InitializeForNewFrame();
		m_pointlightStage.InitializeForNewFrame();
		// TODO: Is there some way to automate these calls so we don't need to remember them in every stage?

		CreateBatches();

		// Light pointers:
		shared_ptr<Light> const keyLight = SceneManager::GetSceneData()->GetKeyLight();
		vector<shared_ptr<Light>> const& pointLights = SceneManager::GetSceneData()->GetPointLights();

		// Add GBuffer textures as stage inputs:		
		shared_ptr<GBufferGraphicsSystem> gBufferGS = std::dynamic_pointer_cast<GBufferGraphicsSystem>(
			RenderManager::Get()->GetGraphicsSystem<GBufferGraphicsSystem>());
		SEAssert("GBuffer GS not found", gBufferGS != nullptr);

		for (size_t i = 0; i < (GBufferGraphicsSystem::GBufferTexNames.size() - 1); i++) // -1, since we handle depth @end
		{
			if (GBufferGraphicsSystem::GBufferTexNames[i] == "GBufferEmissive")
			{
				// Skip the emissive texture since we don't use it in the lighting shaders
				// -> Currently, we assert when trying to bind textures by name to a shader, if the name is not found...
				// TODO: Handle this more elegantly
				continue;
			}

			if (AmbientIsValid())
			{
				m_ambientStage.SetTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[i],
					gBufferGS->GetFinalTextureTargetSet().ColorTarget(i).GetTexture(),
					Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
			}
			if (keyLight)
			{
				m_keylightStage.SetTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[i],
					gBufferGS->GetFinalTextureTargetSet().ColorTarget(i).GetTexture(),
					Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
			}
			if (!pointLights.empty())
			{
				m_pointlightStage.SetTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[i],
					gBufferGS->GetFinalTextureTargetSet().ColorTarget(i).GetTexture(),
					Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
			}
		}

		if (AmbientIsValid())
		{
			// Add IBL texture inputs for ambient stage:
			m_ambientStage.SetTextureInput(
				"CubeMap0",
				m_IEMTex,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear)
			);

			m_ambientStage.SetTextureInput(
				"CubeMap1",
				m_PMREMTex,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearMipMapLinearLinear)
			);

			m_ambientStage.SetTextureInput(
				"Tex7",
				m_BRDF_integrationMap,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::ClampNearestNearest)
			);
		}
		
		if (keyLight)
		{
			// Keylight shadowmap:		
			ShadowMap* const keyLightShadowMap = keyLight->GetShadowMap();
			SEAssert("Key light shadow map is null", keyLightShadowMap != nullptr);

			// Set the key light shadow map:
			shared_ptr<Texture> keylightDepthTex = 
				keyLightShadowMap->GetTextureTargetSet().DepthStencilTarget().GetTexture();
			m_keylightStage.SetTextureInput(
				"Depth0",
				keylightDepthTex,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
		}
	}


	void DeferredLightingGraphicsSystem::CreateBatches()
	{
		// Ambient stage batches:
		const Batch ambeintFullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr, nullptr);
		m_ambientStage.AddBatch(ambeintFullscreenQuadBatch);

		// Keylight stage batches:
		shared_ptr<Light> const keyLight = SceneManager::GetSceneData()->GetKeyLight();
		if (keyLight)
		{
			Batch keylightFullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr, nullptr);

			LightParams keylightParams = GetLightParamData(keyLight);
			shared_ptr<re::ParameterBlock> keylightPB = re::ParameterBlock::Create(
				"LightParams",
				keylightParams,
				re::ParameterBlock::UpdateType::Immutable,
				re::ParameterBlock::Lifetime::SingleFrame);

			keylightFullscreenQuadBatch.AddBatchParameterBlock(keylightPB);

			m_keylightStage.AddBatch(keylightFullscreenQuadBatch);
		}

		// Pointlight stage batches:
		vector<shared_ptr<Light>> const& pointLights = SceneManager::GetSceneData()->GetPointLights();
		for (size_t i = 0; i < pointLights.size(); i++)
		{
			
			Batch pointlightBatch = Batch(m_sphereMeshes[i], nullptr, nullptr);

			// Point light params:
			LightParams pointlightParams = GetLightParamData(pointLights[i]);
			shared_ptr<re::ParameterBlock> pointlightPB = re::ParameterBlock::Create(
				"LightParams", 
				pointlightParams, 
				re::ParameterBlock::UpdateType::Immutable, 
				re::ParameterBlock::Lifetime::SingleFrame);

			pointlightBatch.AddBatchParameterBlock(pointlightPB);

			// Point light mesh params:
			shared_ptr<ParameterBlock> pointlightMeshParams = ParameterBlock::Create(
				"InstancedMeshParams",
				m_sphereMeshes[i]->GetTransform()->GetGlobalMatrix(Transform::TRS),
				ParameterBlock::UpdateType::Immutable,
				ParameterBlock::Lifetime::SingleFrame);

			pointlightBatch.AddBatchParameterBlock(pointlightMeshParams);

			// Batch uniforms:
			ShadowMap* const shadowMap = pointLights[i]->GetShadowMap();
			if (shadowMap != nullptr)
			{
				std::shared_ptr<gr::Texture> const depthTexture = 
					shadowMap->GetTextureTargetSet().DepthStencilTarget().GetTexture();

				pointlightBatch.AddBatchUniform<shared_ptr<gr::Texture>>(
					"CubeMap0", depthTexture, gr::Shader::UniformType::Texture, 1);

				// Our template function expects a shared_ptr to a non-const type; cast it here even though it's gross
				std::shared_ptr<gr::Sampler> const sampler = 
					std::const_pointer_cast<gr::Sampler>(Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));

				pointlightBatch.AddBatchUniform<shared_ptr<gr::Sampler>>(
					"CubeMap0", 
					sampler,
					gr::Shader::UniformType::Sampler, 
					1);
			}			

			// Finally, add the completed batch:
			m_pointlightStage.AddBatch(pointlightBatch);
		}
	}
}