//#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>

#include "GraphicsSystem_DeferredLighting.h"
#include "SceneManager.h"
#include "CoreEngine.h"
#include "Light.h"
#include "ShadowMap.h"
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
using glm::mat4;


namespace gr
{
	DeferredLightingGraphicsSystem::DeferredLightingGraphicsSystem(string name) : GraphicsSystem(name), NamedObject(name),
		m_ambientStage("Ambient light stage"),
		m_keylightStage("Keylight stage"),
		m_pointlightStage("Pointlight stage"),
		m_BRDF_integrationMap(nullptr)
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


		// Ambient lights are not supported by GLTF 2.0; Instead, we just check for a \IBL\ibl.hdr file

		// Attempt to load the source IBL image (gets a pink error image if it fails)
		const string sceneIBLPath = en::CoreEngine::GetConfig()->GetValue<string>("sceneIBLPath");
		shared_ptr<Texture> iblTexture =
			CoreEngine::GetSceneManager()->GetSceneData()->GetLoadTextureByPath({ sceneIBLPath }, false);
		if (!iblTexture)
		{
			const string defaultIBLPath = en::CoreEngine::GetConfig()->GetValue<string>("defaultIBLPath");
			iblTexture = CoreEngine::GetSceneManager()->GetSceneData()->GetLoadTextureByPath({ defaultIBLPath }, true);
		}


		// Ambient light:
		const uint32_t generatedTexRes = 512; // TODO: Make this user-controllable?

		m_ambientStage.GetStageShader() = make_shared<Shader>(
			en::CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("deferredAmbientLightShaderName"));
		m_ambientStage.GetStageShader()->ShaderKeywords().emplace_back("AMBIENT_IBL");
		m_ambientStage.GetStageShader()->Create();

		// Set shader constants:
		const int maxMipLevel = (int)glm::log2((float)generatedTexRes);
		m_ambientStage.GetStageShader()->SetUniform(
			"maxMipLevel",
			&maxMipLevel,
			platform::Shader::UniformType::Int,
			1);

		m_ambientStage.GetStageCamera() = deferredLightingCam;
		m_ambientStage.SetStageParams(ambientStageParams);

		m_ambientMesh =
		{
			gr::meshfactory::CreateQuad	// Align along near plane
			(
				vec3(-1.0f, 1.0f, -1.0f),	// TL
				vec3(1.0f, 1.0f, -1.0f),	// TR
				vec3(-1.0f, -1.0f, -1.0f),	// BL
				vec3(1.0f, -1.0f, -1.0f)	// BR
			)
		};


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

			// Reuse the ambient mesh; it's a full-screen quad rendered on the near plane
			brdfStage.SetGeometryBatches(&m_ambientMesh);

			// Create a render target texture:			
			Texture::TextureParams brdfParams;
			brdfParams.m_width = generatedTexRes;
			brdfParams.m_height = generatedTexRes;
			brdfParams.m_faces = 1;
			brdfParams.m_texUse = Texture::TextureUse::ColorTarget;
			brdfParams.m_texDimension = Texture::TextureDimension::Texture2D;
			brdfParams.m_texFormat = Texture::TextureFormat::RG16F; // Epic recommends 2 channel, 16-bit floats
			brdfParams.m_texColorSpace = Texture::TextureColorSpace::Linear;
			brdfParams.m_clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
			brdfParams.m_texturePath = "BRDFIntegrationMap";
			brdfParams.m_useMIPs = false;

			m_BRDF_integrationMap = std::make_shared<gr::Texture>(brdfParams);

			brdfStage.GetTextureTargetSet().ColorTarget(0) = m_BRDF_integrationMap;
			brdfStage.GetTextureTargetSet().Viewport() = gr::Viewport(0, 0, generatedTexRes, generatedTexRes);
			brdfStage.GetTextureTargetSet().CreateColorTargets();

			// Stage params:
			RenderStage::RenderStageParams brdfStageParams;
			brdfStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
			brdfStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Disabled;
			brdfStageParams.m_srcBlendMode = platform::Context::BlendMode::One;
			brdfStageParams.m_dstBlendMode = platform::Context::BlendMode::Zero;
			brdfStageParams.m_depthTestMode = platform::Context::DepthTestMode::Always;
			brdfStageParams.m_depthWriteMode = platform::Context::DepthWriteMode::Disabled;

			brdfStage.SetStageParams(brdfStageParams);

			pipeline.AppendSingleFrameRenderStage(brdfStage);
		}

		const string equilinearToCubemapShaderName =
			CoreEngine::GetCoreEngine()->GetConfig()->GetValue<string>("equilinearToCubemapBlitShaderName");

		m_cubeMesh.emplace_back(gr::meshfactory::CreateCube());

		// Common IBL cubemap params:
		Texture::TextureParams cubeParams;
		cubeParams.m_width = generatedTexRes;
		cubeParams.m_height = generatedTexRes;
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


		const vec4 texelSize = iblTexture->GetTexelDimenions();
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

		// 1st frame: Generate an IEM (Irradiance Environment Map) cubemap texture for diffuse irradiance
		{
			shared_ptr<Shader> iemShader = make_shared<gr::Shader>(equilinearToCubemapShaderName);
			iemShader->ShaderKeywords().emplace_back("BLIT_IEM");
			iemShader->Create();

			// IEM-specific texture params:
			cubeParams.m_texturePath = "IEMTexture";
			cubeParams.m_useMIPs = false;
			m_IEMTex = make_shared<Texture>(cubeParams);

			for (uint32_t face = 0; face < 6; face++)
			{
				RenderStage iemStage("IEM generation: Face " + to_string(face + 1) + "/6");

				iemStage.GetStageShader() = iemShader;
				iemStage.SetGeometryBatches(&m_cubeMesh);
				iemStage.SetTextureInput(
					"MatAlbedo",
					iblTexture,
					gr::Sampler::GetSampler(gr::Sampler::SamplerType::ClampLinearMipMapLinearLinear));

				const int numSamples = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("numIEMSamples");
				iemStage.SetPerFrameShaderUniformByValue(
					"numSamples", numSamples, platform::Shader::UniformType::Int, 1);
				iemStage.SetPerFrameShaderUniformByValue(
					"in_projection", cubeProjectionMatrix, platform::Shader::UniformType::Matrix4x4f, 1);
				iemStage.SetPerFrameShaderUniformByValue(
					"in_view", captureViews[face], platform::Shader::UniformType::Matrix4x4f, 1);

				iemStage.GetTextureTargetSet().ColorTarget(0) = m_IEMTex;
				iemStage.GetTextureTargetSet().Viewport() = gr::Viewport(0, 0, generatedTexRes, generatedTexRes);
				iemStage.GetTextureTargetSet().CreateColorTargets();

				iblStageParams.m_textureTargetSetConfig.m_targetFace = face;
				iblStageParams.m_textureTargetSetConfig.m_targetMip = 0;
				iemStage.SetStageParams(iblStageParams);

				pipeline.AppendSingleFrameRenderStage(iemStage);
			}
		}

		// 1st frame: Generate PMREM (Pre-filtered Mip-mapped Radiance Environment Map) cubemap for specular reflections
		{
			shared_ptr<Shader> pmremShader = make_shared<gr::Shader>(equilinearToCubemapShaderName);
			pmremShader->ShaderKeywords().emplace_back("BLIT_PMREM");
			pmremShader->Create();

			// PMREM-specific texture params:
			cubeParams.m_texturePath = "PMREMTexture";
			cubeParams.m_useMIPs = true;
			m_PMREMTex = make_shared<Texture>(cubeParams);

			TextureTargetSet pmremTargetSet("PMREM texture targets");
			pmremTargetSet.ColorTarget(0) = m_PMREMTex;
			pmremTargetSet.Viewport() = gr::Viewport(0, 0, generatedTexRes, generatedTexRes);
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
					pmremStage.SetGeometryBatches(&m_cubeMesh);
					pmremStage.SetTextureInput(
						"MatAlbedo",
						iblTexture,
						gr::Sampler::GetSampler(gr::Sampler::SamplerType::ClampLinearMipMapLinearLinear));

					const int numSamples = CoreEngine::GetCoreEngine()->GetConfig()->GetValue<int>("numPMREMSamples");
					pmremStage.SetPerFrameShaderUniformByValue(
						"numSamples", numSamples, platform::Shader::UniformType::Int, 1);
					pmremStage.SetPerFrameShaderUniformByValue(
						"in_projection", cubeProjectionMatrix, platform::Shader::UniformType::Matrix4x4f, 1);
					pmremStage.SetPerFrameShaderUniformByValue(
						"in_view", captureViews[face], platform::Shader::UniformType::Matrix4x4f, 1);
					const float roughness = (float)currentMipLevel / (float)(numMipLevels - 1);
					pmremStage.SetPerFrameShaderUniformByValue(
						"roughness", roughness, platform::Shader::UniformType::Float, 1);

					pmremStage.GetTextureTargetSet() = pmremTargetSet;

					iblStageParams.m_textureTargetSetConfig.m_targetFace = face;
					iblStageParams.m_textureTargetSetConfig.m_targetMip = currentMipLevel;
					pmremStage.SetStageParams(iblStageParams);

					pipeline.AppendSingleFrameRenderStage(pmremStage);
				}
			}
		}

		// If we made it this far, append the ambient stage:
		pipeline.AppendRenderStage(m_ambientStage);
		// TODO: We should use equirectangular images, instead of bothering to convert to cubemaps for IEM/PMREM


		// Key light:
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
			m_keylightStage.SetStageParams(keylightStageParams);

			m_keylightStage.GetStageShader() = keyLight->GetDeferredLightShader();
			m_keylightStage.GetStageCamera() = deferredLightingCam;

			m_keylightMesh.emplace_back(keyLight->DeferredMesh());

			pipeline.AppendRenderStage(m_keylightStage);
		}


		// Point lights: Draw multiple light volume meshes within a single stage
		vector<shared_ptr<Light>> const& pointLights = en::CoreEngine::GetSceneManager()->GetSceneData()->GetPointLights();
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

			//if (!ambientLight && !keyLight) // Don't clear after 1st light
			if (!iblTexture && !keyLight) // Don't clear after 1st light
			{
				pointlightStageParams.m_targetClearMode = platform::Context::ClearTarget::Color;
			}
			else
			{
				pointlightStageParams.m_targetClearMode = platform::Context::ClearTarget::None;
			}

			pointlightStageParams.m_faceCullingMode = platform::Context::FaceCullingMode::Front; // Cull front faces of light volumes
			m_pointlightStage.SetStageParams(pointlightStageParams);

			// TEMP HAX: Store all pointlight meshes, so we can match them against entries in m_perMeshShaderUniforms
			for (shared_ptr<Light> const pointlight : pointLights)
			{
				m_pointlightInstanceMesh.emplace_back(pointlight->DeferredMesh());
				// Note: need to use the attached DeferredMesh here for now, so we get its transform
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


	void DeferredLightingGraphicsSystem::PreRender(re::StagePipeline& pipeline)
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
		shared_ptr<Light> const keyLight = CoreEngine::GetSceneManager()->GetSceneData()->GetKeyLight();
		vector<shared_ptr<Light>> const& pointLights = CoreEngine::GetSceneManager()->GetSceneData()->GetPointLights();

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
			m_keylightStage.SetPerFrameShaderUniformByValue(
				"lightColor", keyLight->GetColor(), platform::Shader::UniformType::Vec3f, 1);

			// TODO: Rename this as keylightDirWorldSpace
			m_keylightStage.SetPerFrameShaderUniformByValue(
				"keylightWorldDir", keyLight->GetTransform()->ForwardWorld(), platform::Shader::UniformType::Vec3f, 1);

			// TODO: Rename this as keylightDirViewSpace
			m_keylightStage.SetPerFrameShaderUniformByValue(
				"keylightViewDir",
				glm::normalize(m_keylightStage.GetStageCamera()->GetViewMatrix() * vec4(keyLight->GetTransform()->ForwardWorld(), 0.0f)),
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
					&pointLights[lightIdx]->GetTransform()->GetWorldPosition().x, // Returns reference
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