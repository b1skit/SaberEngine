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


namespace
{
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


	constexpr uint32_t k_generatedAmbientIBLTexRes = 1024; // TODO: Make this user-controllable via the config


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

		static constexpr char const* const s_shaderName = "LightParams"; // Not counted towards size of struct
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
				shadowMap->GetTextureTargetSet()->GetDepthStencilTarget().GetTexture()->GetTextureDimenions();

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
	DeferredLightingGraphicsSystem::DeferredLightingGraphicsSystem(string name)
		: GraphicsSystem(name)
		, NamedObject(name)
		, m_ambientStage("Ambient light stage")
		, m_keylightStage("Keylight stage")
		, m_pointlightStage("Pointlight stage")
		, m_BRDF_integrationMap(nullptr)
	{
		// Create a fullscreen quad, for reuse when building batches:
		m_screenAlignedQuad = meshfactory::CreateFullscreenQuad(meshfactory::ZLocation::Near);

		// Cube mesh, for rendering of IBL cubemaps
		m_cubeMeshPrimitive = meshfactory::CreateCube();
	}


	void DeferredLightingGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		GBufferGraphicsSystem* gBufferGS = RenderManager::Get()->GetGraphicsSystem<GBufferGraphicsSystem>();
		SEAssert("GBuffer GS not found", gBufferGS != nullptr);
		
		// Create a shared lighting stage texture target:
		Texture::TextureParams lightTargetParams;
		lightTargetParams.m_width = Config::Get()->GetValue<int>(en::Config::k_windowXResValueName);
		lightTargetParams.m_height = Config::Get()->GetValue<int>(en::Config::k_windowYResValueName);
		lightTargetParams.m_faces = 1;
		lightTargetParams.m_usage = Texture::Usage::ColorTarget;
		lightTargetParams.m_dimension = Texture::Dimension::Texture2D;
		lightTargetParams.m_format = Texture::Format::RGBA32F;
		lightTargetParams.m_colorSpace = Texture::ColorSpace::Linear;
		lightTargetParams.m_clearColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		lightTargetParams.m_addToSceneData = false;

		std::shared_ptr<Texture> outputTexture = re::Texture::Create("DeferredLightTarget", lightTargetParams, false);

		std::shared_ptr<TextureTargetSet> deferredLightingTargetSet =
			re::TextureTargetSet::Create("Deferred lighting target");
		deferredLightingTargetSet->SetColorTarget(0, outputTexture);
		deferredLightingTargetSet->SetDepthStencilTarget(gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget());

		Camera* deferredLightingCam = SceneManager::GetSceneData()->GetMainCamera().get();

		
		// Set the target sets, even if the stages aren't actually used (to ensure they're still valid)
		m_ambientStage.SetTextureTargetSet(deferredLightingTargetSet);
		m_keylightStage.SetTextureTargetSet(deferredLightingTargetSet);
		m_pointlightStage.SetTextureTargetSet(deferredLightingTargetSet);
	
		
		gr::PipelineState ambientStageParams;
		ambientStageParams.SetClearTarget(gr::PipelineState::ClearTarget::Color);
		ambientStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Back); // Ambient and directional lights (currently) use back face culling
		ambientStageParams.SetSrcBlendMode(gr::PipelineState::BlendMode::One); // All deferred lighting is additive
		ambientStageParams.SetDstBlendMode(gr::PipelineState::BlendMode::One);
		ambientStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::LEqual); // Ambient & directional
		ambientStageParams.SetDepthWriteMode(gr::PipelineState::DepthWriteMode::Disabled);

		shared_ptr<Texture> iblTexture = SceneManager::GetSceneData()->GetIBLTexture();

		// 1st frame: Generate the pre-integrated BRDF LUT via a single-frame render stage:
		{
			RenderStage brdfStage("BRDF pre-integration stage");

			brdfStage.SetStageShader(
				re::Shader::Create(Config::Get()->GetValue<string>("BRDFIntegrationMapShaderName")));

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
			brdfParams.m_addToSceneData = false;

			m_BRDF_integrationMap = re::Texture::Create("BRDFIntegrationMap", brdfParams, false);

			std::shared_ptr<re::TextureTargetSet> brdfStageTargets = re::TextureTargetSet::Create("BRDF Stage Targets");

			brdfStageTargets->SetColorTarget(0, m_BRDF_integrationMap);
			brdfStageTargets->Viewport() =
				re::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes);

			brdfStage.SetTextureTargetSet(brdfStageTargets);

			// Stage params:
			gr::PipelineState brdfStageParams;
			brdfStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
			brdfStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Disabled);
			brdfStageParams.SetSrcBlendMode(gr::PipelineState::BlendMode::One);
			brdfStageParams.SetDstBlendMode(gr::PipelineState::BlendMode::Zero);
			brdfStageParams.SetDepthTestMode(gr::PipelineState::DepthTestMode::Always);
			brdfStageParams.SetDepthWriteMode(gr::PipelineState::DepthWriteMode::Disabled);

			brdfStage.SetStagePipelineState(brdfStageParams);

			Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);
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
		cubeParams.m_format = Texture::Format::RGBA16F;
		cubeParams.m_colorSpace = Texture::ColorSpace::Linear;
		cubeParams.m_addToSceneData = false;

		// Common IBL texture generation stage params:
		gr::PipelineState iblStageParams;
		iblStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
		iblStageParams.SetFaceCullingMode(gr::PipelineState::FaceCullingMode::Disabled);
		iblStageParams.SetSrcBlendMode(gr::PipelineState::BlendMode::One);
		iblStageParams.SetDstBlendMode(gr::PipelineState::BlendMode::Zero);
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
			shared_ptr<Shader> iemShader = re::Shader::Create(Config::Get()->GetValue<string>("blitIEMShaderName"));

			// IEM-specific texture params:
			cubeParams.m_useMIPs = false;
			m_IEMTex = re::Texture::Create("IEMTexture", cubeParams, false);

			for (uint32_t face = 0; face < 6; face++)
			{
				RenderStage iemStage("IEM generation: Face " + to_string(face + 1) + "/6");

				iemStage.SetStageShader(iemShader);
				iemStage.SetPerFrameTextureInput(
					"MatAlbedo",
					iblTexture,
					re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::ClampLinearMipMapLinearLinear));

				IEMPMREMGenerationParams iemGenerationParams = GetIEMPMREMGenerationParamsData(0, 1);
				shared_ptr<re::ParameterBlock> iemGenerationPB = re::ParameterBlock::Create(
					IEMPMREMGenerationParams::s_shaderName,
					iemGenerationParams,
					re::ParameterBlock::PBType::SingleFrame);
				iemStage.AddPermanentParameterBlock(iemGenerationPB);
				
				// Construct a camera param block to draw into our cubemap rendering targets:
				cubemapCamParams.g_view = cubemapViews[face];
				shared_ptr<re::ParameterBlock> pb = re::ParameterBlock::Create(
					gr::Camera::CameraParams::s_shaderName,
					cubemapCamParams,
					re::ParameterBlock::PBType::SingleFrame);
				iemStage.AddPermanentParameterBlock(pb);

				std::shared_ptr<re::TextureTargetSet> iemTargets = re::TextureTargetSet::Create("IEM Stage Targets");

				iemTargets->SetColorTarget(0, m_IEMTex);
				iemTargets->Viewport() = re::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes);

				iemStage.SetTextureTargetSet(iemTargets);

				iblStageParams.SetTextureTargetSetConfig({ face, 0});
				iemStage.SetStagePipelineState(iblStageParams);

				iemStage.AddBatch(cubeMeshBatch);

				pipeline.AppendSingleFrameRenderStage(iemStage);
			}
		}

		// 1st frame: Generate PMREM (Pre-filtered Mip-mapped Radiance Environment Map) cubemap for specular reflections
		{
			shared_ptr<Shader> pmremShader = re::Shader::Create(Config::Get()->GetValue<string>("blitPMREMShaderName"));

			// PMREM-specific texture params:
			cubeParams.m_useMIPs = true;
			m_PMREMTex = re::Texture::Create("PMREMTexture", cubeParams, false);

			std::shared_ptr<TextureTargetSet> pmremTargetSet = re::TextureTargetSet::Create("PMREM texture targets");
			pmremTargetSet->SetColorTarget(0, m_PMREMTex);
			pmremTargetSet->Viewport() = re::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes);

			const uint32_t numMipLevels = m_PMREMTex->GetNumMips(); // # of mips we need to render

			for (uint32_t currentMipLevel = 0; currentMipLevel < numMipLevels; currentMipLevel++)
			{
				for (uint32_t face = 0; face < 6; face++)
				{
					RenderStage pmremStage(
						"PMREM generation: Face " + to_string(face + 1) + "/6, MIP " +
						to_string(currentMipLevel + 1) + "/" + to_string(numMipLevels));

					pmremStage.SetStageShader(pmremShader);
					pmremStage.SetPerFrameTextureInput(
						"MatAlbedo",
						iblTexture,
						re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::ClampLinearMipMapLinearLinear));
					
					// Construct a camera param block to draw into our cubemap rendering targets:
					cubemapCamParams.g_view = cubemapViews[face];
					shared_ptr<re::ParameterBlock> pb = re::ParameterBlock::Create(
						gr::Camera::CameraParams::s_shaderName,
						cubemapCamParams,
						re::ParameterBlock::PBType::SingleFrame);
					pmremStage.AddPermanentParameterBlock(pb);

					IEMPMREMGenerationParams pmremGenerationParams = 
						GetIEMPMREMGenerationParamsData(currentMipLevel, numMipLevels);
					shared_ptr<re::ParameterBlock> pmremGenerationPB = re::ParameterBlock::Create(
						IEMPMREMGenerationParams::s_shaderName,
						pmremGenerationParams,
						re::ParameterBlock::PBType::SingleFrame);
					pmremStage.AddPermanentParameterBlock(pmremGenerationPB);

					pmremStage.SetTextureTargetSet(pmremTargetSet);

					iblStageParams.SetTextureTargetSetConfig({ face, currentMipLevel });
					pmremStage.SetStagePipelineState(iblStageParams);

					pmremStage.AddBatch(cubeMeshBatch);

					pipeline.AppendSingleFrameRenderStage(pmremStage);
				}
			}
		}

		
		// Ambient light stage:
		m_ambientStage.SetStageShader(
			re::Shader::Create(Config::Get()->GetValue<string>("deferredAmbientLightShaderName")));

		m_ambientStage.AddPermanentParameterBlock(deferredLightingCam->GetCameraParams());
		m_ambientStage.SetStagePipelineState(ambientStageParams);

		// Ambient parameters:		
		AmbientLightParams ambientLightParams = GetAmbientLightParamData();
		std::shared_ptr<re::ParameterBlock> ambientLightPB = re::ParameterBlock::Create(
			AmbientLightParams::s_shaderName,
			ambientLightParams,
			re::ParameterBlock::PBType::Immutable);

		m_ambientStage.AddPermanentParameterBlock(ambientLightPB);

		// If we made it this far, append the ambient stage:
		pipeline.AppendRenderStage(&m_ambientStage);
		

		// Key light stage:
		shared_ptr<Light> keyLight = SceneManager::GetSceneData()->GetKeyLight();

		gr::PipelineState keylightStageParams(ambientStageParams);
		if (keyLight)
		{
			if (!AmbientIsValid()) // Don't clear after 1st light
			{
				keylightStageParams.SetClearTarget(gr::PipelineState::ClearTarget::Color);
			}
			else
			{
				keylightStageParams.SetClearTarget(gr::PipelineState::ClearTarget::None);
			}
			m_keylightStage.SetStagePipelineState(keylightStageParams);

			m_keylightStage.SetStageShader(
				re::Shader::Create(Config::Get()->GetValue<string>("deferredKeylightShaderName")));

			m_keylightStage.AddPermanentParameterBlock(deferredLightingCam->GetCameraParams());

			pipeline.AppendRenderStage(&m_keylightStage);
		}


		// Point light stage:
		vector<shared_ptr<Light>> const& pointLights = SceneManager::GetSceneData()->GetPointLights();
		if (pointLights.size() > 0)
		{
			m_pointlightStage.AddPermanentParameterBlock(deferredLightingCam->GetCameraParams());

			gr::PipelineState pointlightStageParams(keylightStageParams);

			if (!keyLight && !AmbientIsValid())
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
			m_pointlightStage.SetStagePipelineState(pointlightStageParams);

			m_pointlightStage.SetStageShader(
				re::Shader::Create(Config::Get()->GetValue<string>("deferredPointLightShaderName")));

			pipeline.AppendRenderStage(&m_pointlightStage);

			// Create a sphere mesh for each pointlights:
			for (shared_ptr<Light> pointlight : pointLights)
			{
				m_sphereMeshes.emplace_back(std::make_shared<gr::Mesh>(
					"PointLightDeferredMesh", pointlight->GetTransform(), meshfactory::CreateSphere(1.0f)));
			}
		}
	}


	void DeferredLightingGraphicsSystem::PreRender(re::StagePipeline& pipeline)
	{
		CreateBatches();

		// Light pointers:
		shared_ptr<Light> const keyLight = SceneManager::GetSceneData()->GetKeyLight();
		vector<shared_ptr<Light>> const& pointLights = SceneManager::GetSceneData()->GetPointLights();

		// Add GBuffer textures as stage inputs:		
		GBufferGraphicsSystem* gBufferGS = RenderManager::Get()->GetGraphicsSystem<GBufferGraphicsSystem>();
		SEAssert("GBuffer GS not found", gBufferGS != nullptr);

		for (uint8_t slot = 0; slot < (GBufferGraphicsSystem::GBufferTexNames.size() - 1); slot++) // -1, since we handle depth @end
		{
			if (GBufferGraphicsSystem::GBufferTexNames[slot] == "GBufferEmissive")
			{
				// Skip the emissive texture since we don't use it in the lighting shaders
				// -> Currently, we assert when trying to bind textures by name to a shader, if the name is not found...
				// TODO: Handle this more elegantly
				continue;
			}

			if (AmbientIsValid())
			{
				m_ambientStage.SetPerFrameTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[slot],
					gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
					Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
			}
			if (keyLight)
			{
				m_keylightStage.SetPerFrameTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[slot],
					gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
					Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
			}
			if (!pointLights.empty())
			{
				m_pointlightStage.SetPerFrameTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[slot],
					gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
					Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
			}
		}

		if (AmbientIsValid())
		{
			// Add IBL texture inputs for ambient stage:
			m_ambientStage.SetPerFrameTextureInput(
				"CubeMap0",
				m_IEMTex,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear)
			);

			m_ambientStage.SetPerFrameTextureInput(
				"CubeMap1",
				m_PMREMTex,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearMipMapLinearLinear)
			);

			m_ambientStage.SetPerFrameTextureInput(
				"Tex7",
				m_BRDF_integrationMap,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::ClampNearestNearest)
			);
		}
		
		if (keyLight)
		{
			// Keylight shadowmap:		
			ShadowMap* const keyLightShadowMap = keyLight->GetShadowMap();
			if (keyLightShadowMap)
			{
				// Set the key light shadow map:
				shared_ptr<Texture> keylightDepthTex =
					keyLightShadowMap->GetTextureTargetSet()->GetDepthStencilTarget().GetTexture();
				m_keylightStage.SetPerFrameTextureInput(
					"Depth0",
					keylightDepthTex,
					Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));
			}
		}
	}


	void DeferredLightingGraphicsSystem::CreateBatches()
	{
		// Note: Culling is not (currently) supported. For now, we attempt to draw everything
		
		// Ambient stage batches:
		const Batch ambeintFullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);
		m_ambientStage.AddBatch(ambeintFullscreenQuadBatch);

		// Keylight stage batches:
		shared_ptr<Light> const keyLight = SceneManager::GetSceneData()->GetKeyLight();
		if (keyLight)
		{
			Batch keylightFullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);

			LightParams keylightParams = GetLightParamData(keyLight);
			shared_ptr<re::ParameterBlock> keylightPB = re::ParameterBlock::Create(
				LightParams::s_shaderName,
				keylightParams,
				re::ParameterBlock::PBType::SingleFrame);

			keylightFullscreenQuadBatch.SetParameterBlock(keylightPB);

			m_keylightStage.AddBatch(keylightFullscreenQuadBatch);
		}

		// Pointlight stage batches:
		vector<shared_ptr<Light>> const& pointLights = SceneManager::GetSceneData()->GetPointLights();
		for (size_t i = 0; i < pointLights.size(); i++)
		{
			Batch pointlightBatch = Batch(m_sphereMeshes[i], nullptr);

			// Point light params:
			LightParams pointlightParams = GetLightParamData(pointLights[i]);
			shared_ptr<re::ParameterBlock> pointlightPB = re::ParameterBlock::Create(
				LightParams::s_shaderName,
				pointlightParams, 
				re::ParameterBlock::PBType::SingleFrame);

			pointlightBatch.SetParameterBlock(pointlightPB);

			// Point light mesh params:
			shared_ptr<ParameterBlock> pointlightMeshParams = ParameterBlock::Create(
				re::Batch::InstancedMeshParams::s_shaderName,
				m_sphereMeshes[i]->GetTransform()->GetGlobalMatrix(Transform::TRS),
				ParameterBlock::PBType::SingleFrame);

			pointlightBatch.SetParameterBlock(pointlightMeshParams);

			// Batch textures/samplers:
			ShadowMap* const shadowMap = pointLights[i]->GetShadowMap();
			if (shadowMap != nullptr)
			{
				std::shared_ptr<re::Texture> const depthTexture = 
					shadowMap->GetTextureTargetSet()->GetDepthStencilTarget().GetTexture();

				// Our template function expects a shared_ptr to a non-const type; cast it here even though it's gross
				std::shared_ptr<re::Sampler> const sampler = 
					std::const_pointer_cast<re::Sampler>(Sampler::GetSampler(Sampler::WrapAndFilterMode::WrapLinearLinear));

				pointlightBatch.AddTextureAndSamplerInput("CubeMap0", depthTexture, sampler);
			}			

			// Finally, add the completed batch:
			m_pointlightStage.AddBatch(pointlightBatch);
		}
	}


	std::shared_ptr<re::TextureTargetSet const> DeferredLightingGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_ambientStage.GetTextureTargetSet();
	}
}