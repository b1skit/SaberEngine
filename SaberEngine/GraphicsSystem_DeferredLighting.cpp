#include <glm/glm.hpp>

#include "GraphicsSystem_DeferredLighting.h"
#include "SceneManager.h"
#include "CoreEngine.h"
#include "Light.h"
#include "ShadowMap.h"
#include "RenderStage.h"
#include "GraphicsSystem_GBuffer.h"
#include "Mesh.h"
#include "Batch.h"
#include "ParameterBlock.h"

using gr::Light;
using gr::RenderStage;
using gr::Texture;
using gr::TextureTargetSet;
using gr::ShadowMap;
using re::ParameterBlock;
using re::Batch;
using en::CoreEngine;
using std::string;
using std::shared_ptr;
using std::make_shared;
using std::vector;
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


	LightParams GetLightParamData(shared_ptr<Light const> const light)
	{
		LightParams lightParams;
		memset(&lightParams, 0, sizeof(LightParams)); // Ensure unused elements are zeroed

		lightParams.g_lightColorIntensity = light->GetColor();

		// Type-specific params:
		switch (light->Type())
		{
		case gr::Light::LightType::Directional:
		{
			lightParams.g_lightWorldPos = light->GetTransform()->ForwardWorld(); // WorldPos == Light dir
		}
		break;
		case gr::Light::LightType::Point:
		{
			lightParams.g_lightWorldPos = light->GetTransform()->GetWorldPosition();
		}
		break;
		default:
			SEAssert("Light type does not use this param block", false);
		}
		
		shared_ptr<gr::ShadowMap const> const shadowMap = light->GetShadowMap();
		if (shadowMap)
		{
			lightParams.g_shadowMapTexelSize =
				shadowMap->GetTextureTargetSet().DepthStencilTarget().GetTexture()->GetTextureDimenions();

			lightParams.g_shadowBiasMinMax = shadowMap->MinMaxShadowBias();

			shared_ptr<gr::Camera const> const shadowCam = shadowMap->ShadowCamera();
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
				SEAssert("Light shadow type does not use this param block", false);
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
		m_screenAlignedQuad = gr::meshfactory::CreateQuad	// Align along near plane
		(
			vec3(-1.0f, 1.0f, -1.0f),	// TL
			vec3(1.0f, 1.0f, -1.0f),	// TR
			vec3(-1.0f, -1.0f, -1.0f),	// BL
			vec3(1.0f, -1.0f, -1.0f)	// BR
		);

		// Cube mesh, for rendering of IBL cubemaps
		m_cubeMesh = gr::meshfactory::CreateCube();
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

		std::shared_ptr<Texture> outputTexture = make_shared<Texture>("DeferredLightTarget", lightTargetParams);

		TextureTargetSet deferredLightingTargetSet("Deferred lighting target");
		deferredLightingTargetSet.ColorTarget(0) = outputTexture;
		deferredLightingTargetSet.DepthStencilTarget() = gBufferGS->GetFinalTextureTargetSet().DepthStencilTarget();
		deferredLightingTargetSet.CreateColorDepthStencilTargets();

		shared_ptr<Camera> deferredLightingCam = 
			en::CoreEngine::GetSceneManager()->GetSceneData()->GetMainCamera();

		
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


		// Ambient lights are not supported by GLTF 2.0; Instead, we just check for a \IBL\ibl.hdr file.
		// Attempt to load the source IBL image (gets a pink error image if it fails)
		const string sceneIBLPath = en::CoreEngine::GetConfig()->GetValue<string>("sceneIBLPath");
		shared_ptr<Texture> iblTexture =
			CoreEngine::GetSceneManager()->GetSceneData()->GetLoadTextureByPath({ sceneIBLPath }, false);
		if (!iblTexture)
		{
			const string defaultIBLPath = en::CoreEngine::GetConfig()->GetValue<string>("defaultIBLPath");
			iblTexture = CoreEngine::GetSceneManager()->GetSceneData()->GetLoadTextureByPath({ defaultIBLPath }, true);
		}
		Texture::TextureParams iblParams = iblTexture->GetTextureParams();
		iblParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
		iblTexture->SetTextureParams(iblParams);
		iblTexture->Create();


		// 1st frame: Generate the pre-integrated BRDF LUT via a single-frame render stage:
		{
			RenderStage brdfStage("BRDF pre-integration stage");

			brdfStage.GetStageShader() = make_shared<Shader>(
				CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("BRDFIntegrationMapShaderName"));
			brdfStage.GetStageShader()->Create();

			// Create a render target texture:			
			Texture::TextureParams brdfParams;
			brdfParams.m_width = k_generatedAmbientIBLTexRes;
			brdfParams.m_height = k_generatedAmbientIBLTexRes;
			brdfParams.m_faces = 1;
			brdfParams.m_texUse = Texture::TextureUse::ColorTarget;
			brdfParams.m_texDimension = Texture::TextureDimension::Texture2D;
			brdfParams.m_texFormat = Texture::TextureFormat::RG16F; // Epic recommends 2 channel, 16-bit floats
			brdfParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
			brdfParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
			brdfParams.m_useMIPs = false;

			m_BRDF_integrationMap = std::make_shared<gr::Texture>("BRDFIntegrationMap", brdfParams);

			brdfStage.GetTextureTargetSet().ColorTarget(0) = m_BRDF_integrationMap;
			brdfStage.GetTextureTargetSet().Viewport() = 
				gr::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes);
			brdfStage.GetTextureTargetSet().CreateColorTargets();

			// Stage params:
			RenderStage::RenderStageParams brdfStageParams;
			brdfStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
			brdfStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Disabled;
			brdfStageParams.m_srcBlendMode = platform::Context::BlendMode::One;
			brdfStageParams.m_dstBlendMode = platform::Context::BlendMode::Zero;
			brdfStageParams.m_depthTestMode = platform::Context::DepthTestMode::Always;
			brdfStageParams.m_depthWriteMode = platform::Context::DepthWriteMode::Disabled;

			brdfStage.SetRenderStageParams(brdfStageParams);

			Batch fullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr, nullptr);
			brdfStage.AddBatch(fullscreenQuadBatch);

			pipeline.AppendSingleFrameRenderStage(brdfStage);
		}


		// Common IBL cubemap params:
		Texture::TextureParams cubeParams;
		cubeParams.m_width = k_generatedAmbientIBLTexRes;
		cubeParams.m_height = k_generatedAmbientIBLTexRes;
		cubeParams.m_faces = 6;
		cubeParams.m_texUse = Texture::TextureUse::ColorTarget;
		cubeParams.m_texDimension = Texture::TextureDimension::TextureCubeMap;
		cubeParams.m_texFormat = Texture::TextureFormat::RGB16F;
		cubeParams.m_texColorSpace = Texture::TextureColorSpace::Linear;

		// Common IBL texture generation stage params:
		RenderStage::RenderStageParams iblStageParams;
		iblStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
		iblStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Disabled;
		iblStageParams.m_srcBlendMode = platform::Context::BlendMode::One;
		iblStageParams.m_dstBlendMode = platform::Context::BlendMode::Zero;
		iblStageParams.m_depthTestMode = platform::Context::DepthTestMode::Always;
		iblStageParams.m_depthWriteMode = platform::Context::DepthWriteMode::Disabled;

		const mat4 cubeProjectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		const mat4 captureViews[] =
		{
			// TODO: Move this to a common factory somewhere
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(1.0f,  0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f,  0.0f), vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  1.0f,  0.0f), vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, -1.0f,  0.0f), vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  0.0f,  1.0f), vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f,  0.0f, -1.0f), vec3(0.0f, -1.0f,  0.0f))
		};

		// Create a cube mesh batch, for reuse during the initial frame IBL rendering:
		Batch cubeMeshBatch = Batch(m_cubeMesh.get(), nullptr, nullptr);

		const string equilinearToCubemapShaderName =
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("equilinearToCubemapBlitShaderName");

		// TODO: We should use equirectangular images, instead of bothering to convert to cubemaps for IEM/PMREM

		// 1st frame: Generate an IEM (Irradiance Environment Map) cubemap texture for diffuse irradiance
		{
			shared_ptr<Shader> iemShader = make_shared<gr::Shader>(equilinearToCubemapShaderName);
			iemShader->ShaderKeywords().emplace_back("BLIT_IEM");
			iemShader->Create();

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
					gr::Sampler::GetSampler(gr::Sampler::SamplerType::ClampLinearMipMapLinearLinear));

				const int numSamples = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("numIEMSamples");
				iemStage.SetPerFrameShaderUniform(
					"numSamples", numSamples, platform::Shader::UniformType::Int, 1);
				
				// Construct a camera param block to draw into our cubemap rendering targets:
				// TODO: Construct a camera and handle this implicitely
				Camera::CameraParams camParams;
				camParams.g_view = captureViews[face];
				camParams.g_projection = cubeProjectionMatrix;
				camParams.g_viewProjection = glm::mat4(1.f); // Identity; unused
				camParams.g_invViewProjection = glm::mat4(1.f); // Identity; unused
				camParams.g_cameraWPos = vec3(0.f, 0.f, 0.f); // Unused
				
				shared_ptr<re::ParameterBlock> pb = re::ParameterBlock::Create(
					"CameraParams",
					camParams,
					re::ParameterBlock::UpdateType::Immutable,
					re::ParameterBlock::Lifetime::SingleFrame);
				iemStage.AddPermanentParameterBlock(pb);

				iemStage.GetTextureTargetSet().ColorTarget(0) = m_IEMTex;
				iemStage.GetTextureTargetSet().Viewport() = 
					gr::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes);
				iemStage.GetTextureTargetSet().CreateColorTargets();

				iblStageParams.m_textureTargetSetConfig.m_targetFace = face;
				iblStageParams.m_textureTargetSetConfig.m_targetMip = 0;
				iemStage.SetRenderStageParams(iblStageParams);

				iemStage.AddBatch(cubeMeshBatch);

				pipeline.AppendSingleFrameRenderStage(iemStage);
			}
		}

		// 1st frame: Generate PMREM (Pre-filtered Mip-mapped Radiance Environment Map) cubemap for specular reflections
		{
			shared_ptr<Shader> pmremShader = make_shared<gr::Shader>(equilinearToCubemapShaderName);
			pmremShader->ShaderKeywords().emplace_back("BLIT_PMREM");
			pmremShader->Create();

			// PMREM-specific texture params:
			cubeParams.m_useMIPs = true;
			m_PMREMTex = make_shared<Texture>("PMREMTexture", cubeParams);

			TextureTargetSet pmremTargetSet("PMREM texture targets");
			pmremTargetSet.ColorTarget(0) = m_PMREMTex;
			pmremTargetSet.Viewport() = gr::Viewport(0, 0, k_generatedAmbientIBLTexRes, k_generatedAmbientIBLTexRes);
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
						gr::Sampler::GetSampler(gr::Sampler::SamplerType::ClampLinearMipMapLinearLinear));

					const int numSamples = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("numPMREMSamples");
					pmremStage.SetPerFrameShaderUniform(
						"numSamples", numSamples, platform::Shader::UniformType::Int, 1);
					
					// Construct a camera param block to draw into our cubemap rendering targets:
					// TODO: Construct a camera and handle this implicitely
					Camera::CameraParams camParams;
					camParams.g_view = captureViews[face];
					camParams.g_projection = cubeProjectionMatrix;
					camParams.g_viewProjection = glm::mat4(1.f); // Identity; unused
					camParams.g_invViewProjection = glm::mat4(1.f); // Identity; unused
					camParams.g_cameraWPos = vec3(0.f, 0.f, 0.f); // Unused
					shared_ptr<re::ParameterBlock> pb = re::ParameterBlock::Create(
						"CameraParams",
						camParams,
						re::ParameterBlock::UpdateType::Immutable,
						re::ParameterBlock::Lifetime::SingleFrame);
					pmremStage.AddPermanentParameterBlock(pb);

					const float roughness = (float)currentMipLevel / (float)(numMipLevels - 1);
					pmremStage.SetPerFrameShaderUniform(
						"roughness", roughness, platform::Shader::UniformType::Float, 1);

					pmremStage.GetTextureTargetSet() = pmremTargetSet;

					iblStageParams.m_textureTargetSetConfig.m_targetFace = face;
					iblStageParams.m_textureTargetSetConfig.m_targetMip = currentMipLevel;
					pmremStage.SetRenderStageParams(iblStageParams);

					pmremStage.AddBatch(cubeMeshBatch);

					pipeline.AppendSingleFrameRenderStage(pmremStage);
				}
			}
		}

		
		// Ambient light stage:
		m_ambientStage.GetStageShader() = make_shared<Shader>(
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredAmbientLightShaderName"));
		m_ambientStage.GetStageShader()->ShaderKeywords().emplace_back("AMBIENT_IBL");
		m_ambientStage.GetStageShader()->Create();

		m_ambientStage.GetStageCamera() = deferredLightingCam;
		m_ambientStage.SetRenderStageParams(ambientStageParams);

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
		shared_ptr<Light> keyLight = en::CoreEngine::GetSceneManager()->GetSceneData()->GetKeyLight();

		RenderStage::RenderStageParams keylightStageParams(ambientStageParams);
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
			m_keylightStage.SetRenderStageParams(keylightStageParams);

			m_keylightStage.GetStageShader() = make_shared<Shader>(
				en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredKeylightShaderName"));
			m_keylightStage.GetStageShader()->Create();

			m_keylightStage.GetStageCamera() = deferredLightingCam;

			pipeline.AppendRenderStage(m_keylightStage);
		}


		// Point light stage:
		vector<shared_ptr<Light>> const& pointLights = 
			en::CoreEngine::GetSceneManager()->GetSceneData()->GetPointLights();
		if (pointLights.size() > 0)
		{
			m_pointlightStage.GetStageCamera() = deferredLightingCam;

			RenderStage::RenderStageParams pointlightStageParams(keylightStageParams);

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
			m_pointlightStage.SetRenderStageParams(pointlightStageParams);

			m_pointlightStage.GetStageShader() = make_shared<Shader>(
				en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredPointLightShaderName"));
			m_pointlightStage.GetStageShader()->Create();

			pipeline.AppendRenderStage(m_pointlightStage);
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
		shared_ptr<Light> const keyLight = CoreEngine::GetSceneManager()->GetSceneData()->GetKeyLight();
		vector<shared_ptr<Light>> const& pointLights = CoreEngine::GetSceneManager()->GetSceneData()->GetPointLights();

		// Add GBuffer textures as stage inputs:		
		shared_ptr<GBufferGraphicsSystem> gBufferGS = std::dynamic_pointer_cast<GBufferGraphicsSystem>(
			en::CoreEngine::GetRenderManager()->GetGraphicsSystem<GBufferGraphicsSystem>());
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

		if (AmbientIsValid())
		{
			// Add IBL texture inputs for ambient stage:
			m_ambientStage.SetTextureInput(
				"CubeMap0",
				m_IEMTex,
				Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear)
			);

			m_ambientStage.SetTextureInput(
				"CubeMap1",
				m_PMREMTex,
				Sampler::GetSampler(Sampler::SamplerType::WrapLinearMipMapLinearLinear)
			);

			m_ambientStage.SetTextureInput(
				"Tex7",
				m_BRDF_integrationMap,
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
		}
	}


	void DeferredLightingGraphicsSystem::CreateBatches()
	{
		// Ambient stage batches:
		const Batch ambeintFullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr, nullptr);
		m_ambientStage.AddBatch(ambeintFullscreenQuadBatch);

		// Keylight stage batches:
		shared_ptr<Light> const keyLight = CoreEngine::GetSceneManager()->GetSceneData()->GetKeyLight();
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
		vector<shared_ptr<Light>> const& pointLights = CoreEngine::GetSceneManager()->GetSceneData()->GetPointLights();
		for (shared_ptr<Light> const pointlight : pointLights)
		{			
			Batch pointlightBatch = Batch(pointlight->DeferredMesh().get(), nullptr, nullptr);

			// Point light params:
			LightParams pointlightParams = GetLightParamData(pointlight);
			shared_ptr<re::ParameterBlock> pointlightPB = re::ParameterBlock::Create(
				"LightParams", 
				pointlightParams, 
				re::ParameterBlock::UpdateType::Immutable, 
				re::ParameterBlock::Lifetime::SingleFrame);

			pointlightBatch.AddBatchParameterBlock(pointlightPB);

			// Point light mesh params:
			shared_ptr<ParameterBlock> pointlightMeshParams = ParameterBlock::Create(
				"InstancedMeshParams",
				pointlight->DeferredMesh()->GetTransform().GetWorldMatrix(Transform::WorldModel),
				ParameterBlock::UpdateType::Immutable,
				ParameterBlock::Lifetime::SingleFrame);

			pointlightBatch.AddBatchParameterBlock(pointlightMeshParams);

			// Batch uniforms:
			std::shared_ptr<ShadowMap> shadowMap = pointlight->GetShadowMap();
			if (shadowMap != nullptr)
			{
				std::shared_ptr<gr::Texture> const depthTexture = 
					shadowMap->GetTextureTargetSet().DepthStencilTarget().GetTexture();

				pointlightBatch.AddBatchUniform<shared_ptr<gr::Texture>>(
					"CubeMap0", depthTexture, platform::Shader::UniformType::Texture, 1);

				// Our template function expects a shared_ptr to a non-const type; cast it here even though it's gross
				std::shared_ptr<gr::Sampler> const sampler = 
					std::const_pointer_cast<gr::Sampler>(Sampler::GetSampler(Sampler::SamplerType::WrapLinearLinear));

				pointlightBatch.AddBatchUniform<shared_ptr<gr::Sampler>>(
					"CubeMap0", 
					sampler,
					platform::Shader::UniformType::Sampler, 
					1);
			}			

			// Finally, add the completed batch:
			m_pointlightStage.AddBatch(pointlightBatch);
		}
	}
}