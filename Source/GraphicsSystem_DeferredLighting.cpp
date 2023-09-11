// © 2022 Adam Badke. All rights reserved.
#include "Batch.h"
#include "Config.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "Light.h"
#include "MeshPrimitive.h"
#include "ParameterBlock.h"
#include "SceneManager.h"
#include "SceneManager.h"
#include "ShadowMap.h"
#include "RenderManager.h"
#include "RenderStage.h"

using gr::Light;
using re::Texture;
using gr::ShadowMap;
using re::RenderManager;
using re::ParameterBlock;
using re::Batch;
using re::RenderStage;
using re::TextureTargetSet;
using re::Sampler;
using re::Shader;
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
	constexpr uint32_t k_generatedAmbientIBLTexRes = 1024; // TODO: Make this user-controllable via the config

	struct BRDFIntegrationParams
	{
		glm::uvec4 g_integrationTargetResolution;

		static constexpr char const* const s_shaderName = "BRDFIntegrationParams";
	};

	BRDFIntegrationParams GetBRDFIntegrationParamsData()
	{
		BRDFIntegrationParams brdfIntegrationParams{
			.g_integrationTargetResolution =
				glm::uvec4(k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes, 0, 0)
		};

		return brdfIntegrationParams;
	}

	struct IEMPMREMGenerationParams
	{
		glm::vec4 g_numSamplesRoughness; // .x = numIEMSamples, .y = numPMREMSamples, .z = roughness

		static constexpr char const* const s_shaderName = "IEMPMREMGenerationParams"; // Not counted towards size of struct
	};

	IEMPMREMGenerationParams GetIEMPMREMGenerationParamsData(int currentMipLevel, int numMipLevels)
	{
		IEMPMREMGenerationParams generationParams;

		SEAssert("Mip level params are invalid. These must be reasonable, even if they're not used (i.e. IEM generation)",
			currentMipLevel >= 0 && numMipLevels >= 1);
		const float roughness = static_cast<float>(currentMipLevel) / static_cast<float>(numMipLevels - 1);

		generationParams.g_numSamplesRoughness = glm::vec4(
			static_cast<float>(Config::Get()->GetValue<int>("numIEMSamples")),
			static_cast<float>(Config::Get()->GetValue<int>("numPMREMSamples")),
			roughness,
			0.f
		);
		return generationParams;
	}

	struct AmbientLightParams
	{
		uint32_t g_maxPMREMMip;

		static constexpr char const* const s_shaderName = "AmbientLightParams"; // Not counted towards size of struct
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

		glm::vec4 g_renderTargetResolution;

		static constexpr char const* const s_shaderName = "LightParams"; // Not counted towards size of struct
	};


	LightParams GetLightParamData(shared_ptr<Light> const light, std::shared_ptr<re::TextureTargetSet const> targetSet)
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
				shadowMap->GetTextureTargetSet()->GetDepthStencilTarget()->GetTexture()->GetTextureDimenions();

			lightParams.g_shadowBiasMinMax = shadowMap->GetMinMaxShadowBias();

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

		lightParams.g_renderTargetResolution = targetSet->GetTargetDimensions();

		return lightParams;
	}
}


namespace gr
{
	DeferredLightingGraphicsSystem::DeferredLightingGraphicsSystem(string name)
		: GraphicsSystem(name)
		, NamedObject(name)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_ambientStage = re::RenderStage::CreateGraphicsStage("Ambient light stage", gfxStageParams);
		m_keylightStage = re::RenderStage::CreateGraphicsStage("Keylight stage", gfxStageParams);
		m_pointlightStage = re::RenderStage::CreateGraphicsStage("Pointlight stage", gfxStageParams);

		// Create a fullscreen quad, for reuse when building batches:
		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Far);

		// Cube mesh, for rendering of IBL cubemaps
		m_cubeMeshPrimitive = meshfactory::CreateCube();
	}


	void DeferredLightingGraphicsSystem::CreateResourceGenerationStages(re::StagePipeline& pipeline)
	{
		gr::Light::LightTypeProperties& ambientProperties =
			en::SceneManager::GetSceneData()->GetAmbientLight()->AccessLightTypeProperties(Light::AmbientIBL);

		shared_ptr<Texture> iblTexture = SceneManager::GetSceneData()->GetIBLTexture();

		// 1st frame: Generate the pre-integrated BRDF LUT via a single-frame compute stage:
		{
			re::RenderStage::ComputeStageParams computeStageParams;
			std::shared_ptr<re::RenderStage> brdfStage =
				re::RenderStage::CreateSingleFrameComputeStage("BRDF pre-integration compute stage", computeStageParams);

			brdfStage->SetStageShader(re::Shader::Create(en::ShaderNames::k_generateBRDFIntegrationMapShaderName));

			// Create a render target texture:			
			Texture::TextureParams brdfParams;
			brdfParams.m_width = k_generatedAmbientIBLTexRes;
			brdfParams.m_height = k_generatedAmbientIBLTexRes;
			brdfParams.m_faces = 1;
			brdfParams.m_usage = static_cast<Texture::Usage>(Texture::Usage::ComputeTarget | Texture::Usage::Color);
			brdfParams.m_dimension = Texture::Dimension::Texture2D;
			brdfParams.m_format = Texture::Format::RG16F; // Epic recommends 2 channel, 16-bit floats
			brdfParams.m_colorSpace = Texture::ColorSpace::Linear;
			brdfParams.m_useMIPs = false;
			brdfParams.m_addToSceneData = false;
			brdfParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

			ambientProperties.m_ambient.m_BRDF_integrationMap =
				re::Texture::Create("BRDFIntegrationMap", brdfParams, false);

			std::shared_ptr<re::TextureTargetSet> brdfStageTargets = re::TextureTargetSet::Create("BRDF Stage Targets");

			re::TextureTarget::TargetParams targetParams;

			brdfStageTargets->SetColorTarget(0, ambientProperties.m_ambient.m_BRDF_integrationMap, targetParams);
			brdfStageTargets->SetViewport(re::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes));

			re::TextureTarget::TargetParams::BlendModes brdfBlendModes
			{
				re::TextureTarget::TargetParams::BlendMode::One,
				re::TextureTarget::TargetParams::BlendMode::Zero,
			};
			brdfStageTargets->SetColorTargetBlendModes(1, &brdfBlendModes);

			brdfStage->SetTextureTargetSet(brdfStageTargets);

			// Stage params:
			gr::PipelineState brdfStageParams;
			brdfStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
			brdfStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Disabled);
			brdfStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Always);
			brdfStageParams.SetDepthWriteMode(gr::PipelineState::DepthWriteMode::Disabled);

			brdfStage->SetStagePipelineState(brdfStageParams);

			BRDFIntegrationParams const& brdfIntegrationParams = GetBRDFIntegrationParamsData();
			shared_ptr<re::ParameterBlock> brdfIntegrationPB = re::ParameterBlock::Create(
				BRDFIntegrationParams::s_shaderName,
				brdfIntegrationParams,
				re::ParameterBlock::PBType::SingleFrame);
			brdfStage->AddSingleFrameParameterBlock(brdfIntegrationPB);

			// Add our dispatch information to a compute batch. Note: We use numthreads = (1,1,1)
			re::Batch computeBatch = re::Batch(re::Batch::ComputeParams{
				.m_threadGroupCount = glm::uvec3(k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes, 1u) });

			brdfStage->AddBatch(computeBatch);

			pipeline.AppendSingleFrameRenderStage(std::move(brdfStage));
		}


		// Common IBL cubemap params:
		Texture::TextureParams cubeParams;
		cubeParams.m_width = k_generatedAmbientIBLTexRes;
		cubeParams.m_height = k_generatedAmbientIBLTexRes;
		cubeParams.m_faces = 6;
		cubeParams.m_usage = Texture::Usage::ColorTarget;
		cubeParams.m_dimension = Texture::Dimension::TextureCubeMap;
		cubeParams.m_format = Texture::Format::RGBA16F;
		cubeParams.m_colorSpace = Texture::ColorSpace::Linear;
		cubeParams.m_addToSceneData = false;

		// Common IBL texture generation stage params:
		gr::PipelineState iblStageParams;
		iblStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
		iblStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Disabled);
		iblStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Always);
		iblStageParams.SetDepthWriteMode(gr::PipelineState::DepthWriteMode::Disabled);

		// TODO: Use a camera here; A GS should not be manually computing this
		const mat4 cubeProjectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

		const std::vector<glm::mat4> cubemapViews = gr::Camera::GetCubeViewMatrix(vec3(0));

		// Common cubemap camera rendering params; Just need to update g_view for each face/stage
		Camera::CameraParams cubemapCamParams;
		cubemapCamParams.g_projection = cubeProjectionMatrix;
		cubemapCamParams.g_viewProjection = glm::mat4(1.f); // Identity; unused
		cubemapCamParams.g_invViewProjection = glm::mat4(1.f); // Identity; unused
		cubemapCamParams.g_cameraWPos = vec3(0.f, 0.f, 0.f); // Unused

		// Create a cube mesh batch, for reuse during the initial frame IBL rendering:
		Batch cubeMeshBatch = Batch(m_cubeMeshPrimitive.get(), nullptr);

		// TODO: We should use equirectangular images, instead of bothering to convert to cubemaps for IEM/PMREM

		// 1st frame: Generate an IEM (Irradiance Environment Map) cubemap texture for diffuse irradiance
		{
			shared_ptr<Shader> iemShader = re::Shader::Create(en::ShaderNames::k_generateIEMShaderName);

			const string IEMTextureName = iblTexture->GetName() + "_IEMTexture";

			// IEM-specific texture params:
			cubeParams.m_useMIPs = false;
			ambientProperties.m_ambient.m_IEMTex = re::Texture::Create(IEMTextureName, cubeParams, false);

			for (uint32_t face = 0; face < 6; face++)
			{
				re::RenderStage::GraphicsStageParams gfxStageParams;
				std::shared_ptr<re::RenderStage> iemStage = re::RenderStage::CreateSingleFrameGraphicsStage(
					"IEM generation: Face " + to_string(face + 1) + "/6", gfxStageParams);

				iemStage->SetStageShader(iemShader);
				iemStage->AddTextureInput(
					"Tex0",
					iblTexture,
					re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::ClampLinearMipMapLinearLinear));

				IEMPMREMGenerationParams const& iemGenerationParams = GetIEMPMREMGenerationParamsData(0, 1);
				shared_ptr<re::ParameterBlock> iemGenerationPB = re::ParameterBlock::Create(
					IEMPMREMGenerationParams::s_shaderName,
					iemGenerationParams,
					re::ParameterBlock::PBType::SingleFrame);
				iemStage->AddSingleFrameParameterBlock(iemGenerationPB);

				// Construct a camera param block to draw into our cubemap rendering targets:
				cubemapCamParams.g_view = cubemapViews[face];
				shared_ptr<re::ParameterBlock> pb = re::ParameterBlock::Create(
					gr::Camera::CameraParams::s_shaderName,
					cubemapCamParams,
					re::ParameterBlock::PBType::SingleFrame);
				iemStage->AddSingleFrameParameterBlock(pb);

				std::shared_ptr<re::TextureTargetSet> iemTargets = re::TextureTargetSet::Create("IEM Stage Targets");

				re::TextureTarget::TargetParams::BlendModes iemBlendModes
				{
					re::TextureTarget::TargetParams::BlendMode::One,
					re::TextureTarget::TargetParams::BlendMode::Zero,
				};
				iemTargets->SetColorTargetBlendModes(1, &iemBlendModes);

				re::TextureTarget::TargetParams targetParams;
				targetParams.m_targetFace = face;
				targetParams.m_targetSubesource = 0;

				iemTargets->SetColorTarget(0, ambientProperties.m_ambient.m_IEMTex, targetParams);
				iemTargets->SetViewport(re::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes));

				iemStage->SetTextureTargetSet(iemTargets);

				iemStage->SetStagePipelineState(iblStageParams);

				iemStage->AddBatch(cubeMeshBatch);

				pipeline.AppendSingleFrameRenderStage(std::move(iemStage));
			}
		}

		// 1st frame: Generate PMREM (Pre-filtered Mip-mapped Radiance Environment Map) cubemap for specular reflections
		{
			shared_ptr<Shader> pmremShader = re::Shader::Create(en::ShaderNames::k_generatePMREMShaderName);

			// PMREM-specific texture params:
			const string PMREMTextureName = iblTexture->GetName() + "_PMREMTexture";
			cubeParams.m_useMIPs = true;
			ambientProperties.m_ambient.m_PMREMTex = re::Texture::Create(PMREMTextureName, cubeParams, false);

			const uint32_t numMipLevels = ambientProperties.m_ambient.m_PMREMTex->GetNumMips(); // # of mips we need to render

			for (uint32_t currentMipLevel = 0; currentMipLevel < numMipLevels; currentMipLevel++)
			{
				for (uint32_t face = 0; face < 6; face++)
				{
					const string postFix = to_string(face + 1) + "/6, MIP " +
						to_string(currentMipLevel + 1) + "/" + to_string(numMipLevels);

					re::RenderStage::GraphicsStageParams gfxStageParams;
					std::shared_ptr<re::RenderStage> pmremStage = re::RenderStage::CreateSingleFrameGraphicsStage(
						"PMREM generation: Face " + postFix, gfxStageParams);

					pmremStage->SetStageShader(pmremShader);
					pmremStage->AddTextureInput(
						"Tex0",
						iblTexture,
						re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::ClampLinearMipMapLinearLinear));

					// Construct a camera param block to draw into our cubemap rendering targets:
					cubemapCamParams.g_view = cubemapViews[face];
					shared_ptr<re::ParameterBlock> pb = re::ParameterBlock::Create(
						gr::Camera::CameraParams::s_shaderName,
						cubemapCamParams,
						re::ParameterBlock::PBType::SingleFrame);
					pmremStage->AddSingleFrameParameterBlock(pb);

					IEMPMREMGenerationParams const& pmremGenerationParams =
						GetIEMPMREMGenerationParamsData(currentMipLevel, numMipLevels);
					shared_ptr<re::ParameterBlock> pmremGenerationPB = re::ParameterBlock::Create(
						IEMPMREMGenerationParams::s_shaderName,
						pmremGenerationParams,
						re::ParameterBlock::PBType::SingleFrame);
					pmremStage->AddSingleFrameParameterBlock(pmremGenerationPB);

					re::TextureTarget::TargetParams targetParams;
					targetParams.m_targetFace = face;
					targetParams.m_targetSubesource = currentMipLevel;

					std::shared_ptr<TextureTargetSet> pmremTargetSet =
						re::TextureTargetSet::Create("PMREM texture targets: Face " + postFix);

					pmremTargetSet->SetColorTarget(0, ambientProperties.m_ambient.m_PMREMTex, targetParams);
					pmremTargetSet->SetViewport(
						re::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes));

					re::TextureTarget::TargetParams::BlendModes pmremBlendModes
					{
						re::TextureTarget::TargetParams::BlendMode::One,
						re::TextureTarget::TargetParams::BlendMode::Zero,
					};
					pmremTargetSet->SetColorTargetBlendModes(1, &pmremBlendModes);

					pmremStage->SetTextureTargetSet(pmremTargetSet);

					pmremStage->SetStagePipelineState(iblStageParams);

					pmremStage->AddBatch(cubeMeshBatch);

					pipeline.AppendSingleFrameRenderStage(std::move(pmremStage));
				}
			}
		}
	}


	void DeferredLightingGraphicsSystem::Create(re::RenderSystem& renderSystem, re::StagePipeline& pipeline)
	{
		GBufferGraphicsSystem* gBufferGS = renderSystem.GetGraphicsSystem<GBufferGraphicsSystem>();
		SEAssert("GBuffer GS not found", gBufferGS != nullptr);
		
		// Create a shared lighting stage texture target:
		Texture::TextureParams lightTargetParams;
		lightTargetParams.m_width = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowXResValueName);
		lightTargetParams.m_height = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowYResValueName);
		lightTargetParams.m_faces = 1;
		lightTargetParams.m_usage = Texture::Usage::ColorTarget;
		lightTargetParams.m_dimension = Texture::Dimension::Texture2D;
		lightTargetParams.m_format = Texture::Format::RGBA16F;
		lightTargetParams.m_colorSpace = Texture::ColorSpace::Linear;
		lightTargetParams.m_useMIPs = false;
		lightTargetParams.m_addToSceneData = false;
		lightTargetParams.m_clear.m_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);

		std::shared_ptr<Texture> outputTexture = re::Texture::Create("DeferredLightTarget", lightTargetParams, false);

		re::TextureTarget::TargetParams targetParams;

		std::shared_ptr<TextureTargetSet> deferredLightingTargetSet = 
			re::TextureTargetSet::Create("Deferred light targets");
		deferredLightingTargetSet->SetColorTarget(0, outputTexture, targetParams);
		deferredLightingTargetSet->SetDepthStencilTarget(gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget());
		
		// All deferred lighting is additive
		re::TextureTarget::TargetParams::BlendModes deferredBlendModes
		{
			re::TextureTarget::TargetParams::BlendMode::One,
			re::TextureTarget::TargetParams::BlendMode::One,
		};
		deferredLightingTargetSet->SetColorTargetBlendModes(1, &deferredBlendModes);

		Camera* deferredLightingCam = SceneManager::Get()->GetMainCamera().get();

		
		// Set the target sets, even if the stages aren't actually used (to ensure they're still valid)
		m_ambientStage->SetTextureTargetSet(deferredLightingTargetSet);
		m_keylightStage->SetTextureTargetSet(deferredLightingTargetSet);
		m_pointlightStage->SetTextureTargetSet(deferredLightingTargetSet);

		// We'll be creating the data we need to render the scene's ambient light:
		gr::Light::LightTypeProperties& ambientProperties =
			en::SceneManager::GetSceneData()->GetAmbientLight()->AccessLightTypeProperties(Light::AmbientIBL);

		shared_ptr<Texture> iblTexture = SceneManager::GetSceneData()->GetIBLTexture();
		
		

		const bool ambientIsValid =
			ambientProperties.m_ambient.m_BRDF_integrationMap &&
			ambientProperties.m_ambient.m_IEMTex &&
			ambientProperties.m_ambient.m_PMREMTex;

		gr::PipelineState ambientStageParams;
		ambientStageParams.SetClearTarget(gr::PipelineState::ClearTarget::Color);

		// Ambient/directional lights use back face culling, as they're fullscreen quads
		ambientStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back); 

		// Our fullscreen quad is on the far plane; We only want to light something if the quad is behind the geo (i.e.
		// the quad's depth is greater than what is in the depth buffer)
		ambientStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Greater);
		ambientStageParams.SetDepthWriteMode(gr::PipelineState::DepthWriteMode::Disabled);
		
		// Ambient light stage:
		m_ambientStage->SetStageShader(re::Shader::Create(en::ShaderNames::k_deferredAmbientLightShaderName));

		m_ambientStage->AddPermanentParameterBlock(deferredLightingCam->GetCameraParams());
		m_ambientStage->SetStagePipelineState(ambientStageParams);

		// Ambient parameters:		
		AmbientLightParams ambientLightParams = GetAmbientLightParamData();
		std::shared_ptr<re::ParameterBlock> ambientLightPB = re::ParameterBlock::Create(
			AmbientLightParams::s_shaderName,
			ambientLightParams,
			re::ParameterBlock::PBType::Immutable);

		m_ambientStage->AddPermanentParameterBlock(ambientLightPB);

		// If we made it this far, append the ambient stage:
		pipeline.AppendRenderStage(m_ambientStage);
		

		// Key light stage:
		shared_ptr<Light> keyLight = SceneManager::GetSceneData()->GetKeyLight();

		gr::PipelineState keylightStageParams(ambientStageParams);
		if (keyLight)
		{
			if (!ambientIsValid) // Don't clear after 1st light
			{
				keylightStageParams.SetClearTarget(gr::PipelineState::ClearTarget::Color);
			}
			else
			{
				keylightStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
			}
			m_keylightStage->SetStagePipelineState(keylightStageParams);

			m_keylightStage->SetStageShader(re::Shader::Create(en::ShaderNames::k_deferredDirectionalLightShaderName));

			m_keylightStage->AddPermanentParameterBlock(deferredLightingCam->GetCameraParams());

			pipeline.AppendRenderStage(m_keylightStage);
		}


		// Point light stage:
		vector<shared_ptr<Light>> const& pointLights = SceneManager::GetSceneData()->GetPointLights();
		if (pointLights.size() > 0)
		{
			m_pointlightStage->AddPermanentParameterBlock(deferredLightingCam->GetCameraParams());

			gr::PipelineState pointlightStageParams(keylightStageParams);

			if (!keyLight && !ambientIsValid)
			{
				keylightStageParams.SetClearTarget(gr::PipelineState::ClearTarget::Color);
			}

			// Pointlights only illuminate something if the sphere volume is behind it
			pointlightStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::GEqual);

			if (!iblTexture && !keyLight) // Don't clear after 1st light
			{
				pointlightStageParams.SetClearTarget(gr::PipelineState::ClearTarget::Color);
			}
			else
			{
				pointlightStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
			}

			pointlightStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Front); // Cull front faces of light volumes
			m_pointlightStage->SetStagePipelineState(pointlightStageParams);

			m_pointlightStage->SetStageShader(re::Shader::Create(en::ShaderNames::k_deferredPointLightShaderName));

			pipeline.AppendRenderStage(m_pointlightStage);

			// Create a sphere mesh for each pointlights:
			for (shared_ptr<Light> pointlight : pointLights)
			{
				m_sphereMeshes.emplace_back(std::make_shared<gr::Mesh>(
					"PointLightDeferredMesh", pointlight->GetTransform(), meshfactory::CreateSphere(1.0f)));
			}
		}


		// Attach GBuffer color inputs:
		constexpr uint8_t numGBufferColorInputs =
			static_cast<uint8_t>(GBufferGraphicsSystem::GBufferTexNames.size() - 1);
		for (uint8_t slot = 0; slot < numGBufferColorInputs; slot++)
		{
			if (GBufferGraphicsSystem::GBufferTexNames[slot] == "GBufferEmissive")
			{
				// Skip the emissive texture since we don't use it in the lighting shaders
				// -> Currently, we assert when trying to bind textures by name to a shader, if the name is not found...
				// TODO: Handle this more elegantly
				continue;
			}

			if (ambientIsValid)
			{
				m_ambientStage->AddTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[slot],
					gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
					Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
			}
			if (keyLight)
			{
				m_keylightStage->AddTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[slot],
					gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
					Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
			}
			if (!pointLights.empty())
			{
				m_pointlightStage->AddTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[slot],
					gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
					Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
			}
		}


		// Attach the GBUffer depth input:
		constexpr uint8_t depthBufferSlot = gr::GBufferGraphicsSystem::GBufferDepth;

		if (keyLight)
		{
			m_keylightStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
				gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
				Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));

			// Keylight shadowmap:
			ShadowMap* const keyLightShadowMap = keyLight->GetShadowMap();
			if (keyLightShadowMap)
			{
				// Set the key light shadow map:
				shared_ptr<Texture> keylightShadowMapTex =
					keyLightShadowMap->GetTextureTargetSet()->GetDepthStencilTarget()->GetTexture();
				m_keylightStage->AddTextureInput(
					"Depth0",
					keylightShadowMapTex,
					Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
			}
		}

		if (!pointLights.empty())
		{
			m_pointlightStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
				gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
				Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
		}

		if (ambientIsValid)
		{
			m_ambientStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
				gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
				Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));

			// Add IBL texture inputs for ambient stage:
			m_ambientStage->AddTextureInput(
				"CubeMap0",
				ambientProperties.m_ambient.m_IEMTex,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear)
			);

			m_ambientStage->AddTextureInput(
				"CubeMap1",
				ambientProperties.m_ambient.m_PMREMTex,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearMipMapLinearLinear)
			);

			m_ambientStage->AddTextureInput(
				"Tex7",
				ambientProperties.m_ambient.m_BRDF_integrationMap,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::ClampNearestNearest)
			);
		}
	}


	void DeferredLightingGraphicsSystem::PreRender()
	{
		CreateBatches();		
	}


	void DeferredLightingGraphicsSystem::CreateBatches()
	{
		// Note: Culling is not (currently) supported. For now, we attempt to draw everything
		
		// Ambient stage batches:
		const Batch ambientFullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);
		m_ambientStage->AddBatch(ambientFullscreenQuadBatch);

		// Keylight stage batches:
		shared_ptr<Light> const keyLight = SceneManager::GetSceneData()->GetKeyLight();
		if (keyLight)
		{
			Batch keylightFullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);

			LightParams keylightParams = GetLightParamData(keyLight, m_keylightStage->GetTextureTargetSet());
			shared_ptr<re::ParameterBlock> keylightPB = re::ParameterBlock::Create(
				LightParams::s_shaderName,
				keylightParams,
				re::ParameterBlock::PBType::SingleFrame);

			keylightFullscreenQuadBatch.SetParameterBlock(keylightPB);

			m_keylightStage->AddBatch(keylightFullscreenQuadBatch);
		}

		// Pointlight stage batches:
		vector<shared_ptr<Light>> const& pointLights = SceneManager::GetSceneData()->GetPointLights();
		for (size_t i = 0; i < pointLights.size(); i++)
		{
			Batch pointlightBatch = Batch(m_sphereMeshes[i], nullptr);

			// Point light params:
			LightParams pointlightParams = GetLightParamData(pointLights[i], m_pointlightStage->GetTextureTargetSet());
			shared_ptr<re::ParameterBlock> pointlightPB = re::ParameterBlock::Create(
				LightParams::s_shaderName,
				pointlightParams, 
				re::ParameterBlock::PBType::SingleFrame);

			pointlightBatch.SetParameterBlock(pointlightPB);

			// Point light mesh params:
			shared_ptr<ParameterBlock> pointlightMeshParams = 
				gr::Mesh::CreateInstancedMeshParamsData(m_sphereMeshes[i]->GetTransform());

			pointlightBatch.SetParameterBlock(pointlightMeshParams);

			// Batch textures/samplers:
			ShadowMap* const shadowMap = pointLights[i]->GetShadowMap();
			if (shadowMap != nullptr)
			{
				std::shared_ptr<re::Texture> const depthTexture = 
					shadowMap->GetTextureTargetSet()->GetDepthStencilTarget()->GetTexture();

				// Our template function expects a shared_ptr to a non-const type; cast it here even though it's gross
				std::shared_ptr<re::Sampler> const sampler = 
					std::const_pointer_cast<re::Sampler>(Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));

				pointlightBatch.AddTextureAndSamplerInput("CubeMap0", depthTexture, sampler);
			}			

			// Finally, add the completed batch:
			m_pointlightStage->AddBatch(pointlightBatch);
		}
	}


	std::shared_ptr<re::TextureTargetSet const> DeferredLightingGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_ambientStage->GetTextureTargetSet();
	}
}