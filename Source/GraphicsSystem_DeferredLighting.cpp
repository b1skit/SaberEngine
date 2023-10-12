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
	struct BRDFIntegrationParams
	{
		glm::uvec4 g_integrationTargetResolution;

		static constexpr char const* const s_shaderName = "BRDFIntegrationParams";
	};

	BRDFIntegrationParams GetBRDFIntegrationParamsData()
	{
		const uint32_t brdfTexWidthHeight =
			static_cast<uint32_t>(Config::Get()->GetValue<int>(en::ConfigKeys::k_brdfLUTWidthHeight));

		BRDFIntegrationParams brdfIntegrationParams{
			.g_integrationTargetResolution =
				glm::uvec4(brdfTexWidthHeight, brdfTexWidthHeight, 0, 0)
		};

		return brdfIntegrationParams;
	}


	struct IEMPMREMGenerationParams
	{
		glm::vec4 g_numSamplesRoughnessFaceIdx; // .x = numIEMSamples, .y = numPMREMSamples, .z = roughness, .w = faceIdx
		glm::vec4 g_mipLevelSrcWidthSrcHeightSrcNumMips; // .x = IEM mip level, .yz = src width/height, .w = num src mips

		static constexpr char const* const s_shaderName = "IEMPMREMGenerationParams"; // Not counted towards size of struct
	};

	IEMPMREMGenerationParams GetIEMPMREMGenerationParamsData(
		int currentMipLevel, int numMipLevels, uint32_t faceIdx, uint32_t srcWidth, uint32_t srcHeight)
	{
		IEMPMREMGenerationParams generationParams;

		SEAssert("Mip level params are invalid. These must be reasonable, even if they're not used (i.e. IEM generation)",
			currentMipLevel >= 0 && numMipLevels >= 1);

		float roughness;
		if (numMipLevels > 1)
		{
			roughness = static_cast<float>(currentMipLevel) / static_cast<float>(numMipLevels - 1);
		}
		else
		{
			roughness = 0;
		}

		const int numIEMSamples = Config::Get()->GetValue<int>(en::ConfigKeys::k_iemNumSamples);
		const int numPMREMSamples = Config::Get()->GetValue<int>(en::ConfigKeys::k_pmremNumSamples);

		generationParams.g_numSamplesRoughnessFaceIdx = glm::vec4(
			static_cast<float>(numIEMSamples),
			static_cast<float>(numPMREMSamples),
			roughness,
			faceIdx);

		// We sample a lower mip level to approximate a Gaussian blur of the input image (i.e. low-pass filter), 
		// significantly reducing the required number of samples required to get a noise free convolution.
		// Empirical testing shows that for N = 4096 IEM samples per pixel, a 128x64 src image gives reasonable
		// results.
		// We assume our IBL inputs are roughly 2:1 in dimensions, and compute the src mip from the maximum dimension
		const float maxDimension = static_cast<float>(std::max(srcWidth, srcHeight));
		constexpr uint32_t k_targetMaxDimension = 128;
		generationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips = glm::vec4(
			glm::log2(maxDimension / k_targetMaxDimension), 
			srcWidth, 
			srcHeight, 
			numMipLevels);

		return generationParams;
	}


	struct AmbientLightParams
	{
		// .x = max PMREM mip level, .y = pre-integrated DFG texture width/height, .z diffuse scale, .w = specular scale
		glm::vec4 g_maxPMREMMipDFGResScaleDiffuseScaleSpec;

		static constexpr char const* const s_shaderName = "AmbientLightParams"; // Not counted towards size of struct
	};


	AmbientLightParams GetAmbientLightParamsData(uint32_t numPMREMMips, float diffuseScale, float specularScale)
	{
		AmbientLightParams ambientLightParams;

		const uint32_t dfgTexWidthHeight = 
			static_cast<uint32_t>(Config::Get()->GetValue<int>(en::ConfigKeys::k_brdfLUTWidthHeight));

		const uint32_t maxPMREMMipLevel = numPMREMMips - 1;

		ambientLightParams.g_maxPMREMMipDFGResScaleDiffuseScaleSpec = glm::vec4(
			maxPMREMMipLevel,
			dfgTexWidthHeight,
			diffuseScale,
			specularScale);

		return ambientLightParams;
	}

	
	struct LightParams
	{
		glm::vec4 g_lightColorIntensity; // .rgb = hue, .a = intensity

		// .xyz = world pos (Directional lights: Normalized point -> source dir)
		// .w = emitter radius (point lights)
		glm::vec4 g_lightWorldPosRadius; 

		glm::vec4 g_shadowMapTexelSize;	// .xyzw = width, height, 1/width, 1/height
		glm::vec4 g_shadowCamNearFarBiasMinMax; // .xy = shadow cam near/far, .zw = min, max shadow bias

		glm::mat4 g_shadowCam_VP;

		glm::vec4 g_renderTargetResolution; // .xy = xRes, yRes, .zw = 1/xRes 1/yRes
		glm::vec4 g_intensityScale; // .xy = diffuse/specular intensity scale, .zw = unused

		static constexpr char const* const s_shaderName = "LightParams"; // Not counted towards size of struct
	};


	LightParams GetLightParamData(shared_ptr<Light> const light, std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		Light::LightTypeProperties const& lightProperties = light->AccessLightTypeProperties(light->Type());

		LightParams lightParams;
		memset(&lightParams, 0, sizeof(LightParams)); // Ensure unused elements are zeroed

		lightParams.g_lightColorIntensity = light->GetColorIntensity();

		// Type-specific params:
		switch (light->Type())
		{
		case gr::Light::LightType::Directional:
		{
			lightParams.g_lightWorldPosRadius = 
				glm::vec4(light->GetTransform()->GetGlobalForward(), 0.f); // WorldPos == Light dir
		}
		break;
		case gr::Light::LightType::Point:
		{
			lightParams.g_lightWorldPosRadius = glm::vec4(
				light->GetTransform()->GetGlobalPosition(), 
				lightProperties.m_point.m_emitterRadius);
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

			gr::Camera* const shadowCam = shadowMap->ShadowCamera();
			lightParams.g_shadowCamNearFarBiasMinMax = glm::vec4(
				shadowCam->NearFar(),
				shadowMap->GetMinMaxShadowBias());

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
		
		lightParams.g_intensityScale = glm::vec4(
			static_cast<float>(lightProperties.m_diffuseEnabled),
			static_cast<float>(lightProperties.m_specularEnabled),
			0.f,
			0.f);

		return lightParams;
	}
}


namespace gr
{
	constexpr char const* k_gsName = "Deferred Lighting Graphics System";


	DeferredLightingGraphicsSystem::DeferredLightingGraphicsSystem()
		: GraphicsSystem(k_gsName)
		, NamedObject(k_gsName)
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

			const uint32_t brdfTexWidthHeight = 
				static_cast<uint32_t>(Config::Get()->GetValue<int>(en::ConfigKeys::k_brdfLUTWidthHeight));

			// Create a render target texture:			
			Texture::TextureParams brdfParams;
			brdfParams.m_width = brdfTexWidthHeight;
			brdfParams.m_height = brdfTexWidthHeight;
			brdfParams.m_faces = 1;
			brdfParams.m_usage = static_cast<Texture::Usage>(Texture::Usage::ComputeTarget | Texture::Usage::Color);
			brdfParams.m_dimension = Texture::Dimension::Texture2D;
			brdfParams.m_format = Texture::Format::RGBA16F;
			brdfParams.m_colorSpace = Texture::ColorSpace::Linear;
			brdfParams.m_mipMode = re::Texture::MipMode::None;
			brdfParams.m_addToSceneData = false;
			brdfParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

			ambientProperties.m_ambient.m_BRDF_integrationMap =
				re::Texture::Create("BRDFIntegrationMap", brdfParams, false);

			std::shared_ptr<re::TextureTargetSet> brdfStageTargets = re::TextureTargetSet::Create("BRDF Stage Targets");

			re::TextureTarget::TargetParams colorTargetParams;

			brdfStageTargets->SetColorTarget(0, ambientProperties.m_ambient.m_BRDF_integrationMap, colorTargetParams);
			brdfStageTargets->SetViewport(re::Viewport(0, 0, brdfTexWidthHeight, brdfTexWidthHeight));
			brdfStageTargets->SetScissorRect(re::ScissorRect(0, 0, brdfTexWidthHeight, brdfTexWidthHeight));

			re::TextureTarget::TargetParams::BlendModes brdfBlendModes
			{
				re::TextureTarget::TargetParams::BlendMode::One,
				re::TextureTarget::TargetParams::BlendMode::Zero,
			};
			brdfStageTargets->SetColorTargetBlendModes(1, &brdfBlendModes);

			brdfStage->SetTextureTargetSet(brdfStageTargets);

			// Stage params:
			re::PipelineState brdfStageParams;
			brdfStageParams.SetClearTarget(re::PipelineState::ClearTarget::None);
			brdfStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
			brdfStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);

			brdfStage->SetStagePipelineState(brdfStageParams);

			BRDFIntegrationParams const& brdfIntegrationParams = GetBRDFIntegrationParamsData();
			shared_ptr<re::ParameterBlock> brdfIntegrationPB = re::ParameterBlock::Create(
				BRDFIntegrationParams::s_shaderName,
				brdfIntegrationParams,
				re::ParameterBlock::PBType::SingleFrame);
			brdfStage->AddSingleFrameParameterBlock(brdfIntegrationPB);

			// Add our dispatch information to a compute batch. Note: We use numthreads = (1,1,1)
			re::Batch computeBatch = re::Batch(re::Batch::ComputeParams{
				.m_threadGroupCount = glm::uvec3(brdfTexWidthHeight, brdfTexWidthHeight, 1u) });

			brdfStage->AddBatch(computeBatch);

			pipeline.AppendSingleFrameRenderStage(std::move(brdfStage));
		}

		// Common IBL texture generation stage params:
		re::PipelineState iblStageParams;
		iblStageParams.SetClearTarget(re::PipelineState::ClearTarget::None);
		iblStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
		iblStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);

		// Build some camera params for rendering the 6 faces of a cubemap
		const mat4 cubeProjectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

		std::vector<glm::mat4> const& cubemapViews = gr::Camera::BuildCubeViewMatrices(vec3(0));

		// Common cubemap camera rendering params; Just need to update g_view for each face/stage
		Camera::CameraParams cubemapCamParams{};
		cubemapCamParams.g_projection = cubeProjectionMatrix;
		cubemapCamParams.g_viewProjection = glm::mat4(1.f); // Identity; unused
		cubemapCamParams.g_invViewProjection = glm::mat4(1.f); // Identity; unused
		cubemapCamParams.g_cameraWPos = vec3(0.f, 0.f, 0.f); // Unused

		// Create a cube mesh batch, for reuse during the initial frame IBL rendering:
		Batch cubeMeshBatch = Batch(m_cubeMeshPrimitive.get(), nullptr);

		// TODO: We should use equirectangular images, instead of bothering to convert to cubemaps for IEM/PMREM
		// -> Need to change the HLSL Get___DominantDir functions to ensure the result is normalized


		// 1st frame: Generate an IEM (Irradiance Environment Map) cubemap texture for diffuse irradiance
		{
			const uint32_t iemTexWidthHeight =
				static_cast<uint32_t>(Config::Get()->GetValue<int>(en::ConfigKeys::k_iemTexWidthHeight));

			// IEM-specific texture params:
			Texture::TextureParams iemTexParams;
			iemTexParams.m_width = iemTexWidthHeight;
			iemTexParams.m_height = iemTexWidthHeight;
			iemTexParams.m_faces = 6;
			iemTexParams.m_usage = Texture::Usage::ColorTarget;
			iemTexParams.m_dimension = Texture::Dimension::TextureCubeMap;
			iemTexParams.m_format = Texture::Format::RGBA16F;
			iemTexParams.m_colorSpace = Texture::ColorSpace::Linear;
			iemTexParams.m_addToSceneData = false;
			iemTexParams.m_mipMode = re::Texture::MipMode::None;

			shared_ptr<Shader> iemShader = re::Shader::Create(en::ShaderNames::k_generateIEMShaderName);

			const string IEMTextureName = iblTexture->GetName() + "_IEMTexture";
			ambientProperties.m_ambient.m_IEMTex = re::Texture::Create(IEMTextureName, iemTexParams, false);

			for (uint32_t face = 0; face < 6; face++)
			{
				re::RenderStage::GraphicsStageParams gfxStageParams;
				std::shared_ptr<re::RenderStage> iemStage = re::RenderStage::CreateSingleFrameGraphicsStage(
					"IEM generation: Face " + to_string(face + 1) + "/6", gfxStageParams);

				iemStage->SetStageShader(iemShader);
				iemStage->AddTextureInput(
					"Tex0",
					iblTexture,
					re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Clamp_LinearMipMapLinear_Linear));

				IEMPMREMGenerationParams const& iemGenerationParams = 
					GetIEMPMREMGenerationParamsData(0, 1, face, iblTexture->Width(), iblTexture->Height());
				shared_ptr<re::ParameterBlock> iemGenerationPB = re::ParameterBlock::Create(
					IEMPMREMGenerationParams::s_shaderName,
					iemGenerationParams,
					re::ParameterBlock::PBType::SingleFrame);
				iemStage->AddSingleFrameParameterBlock(iemGenerationPB);

				// Construct a camera param block to draw into our cubemap rendering targets:
				cubemapCamParams.g_view = cubemapViews[face];
				shared_ptr<re::ParameterBlock> cubemapCamParamsPB = re::ParameterBlock::Create(
					gr::Camera::CameraParams::s_shaderName,
					cubemapCamParams,
					re::ParameterBlock::PBType::SingleFrame);
				iemStage->AddSingleFrameParameterBlock(cubemapCamParamsPB);

				std::shared_ptr<re::TextureTargetSet> iemTargets = re::TextureTargetSet::Create("IEM Stage Targets");

				re::TextureTarget::TargetParams::BlendModes iemBlendModes
				{
					re::TextureTarget::TargetParams::BlendMode::One,
					re::TextureTarget::TargetParams::BlendMode::Zero,
				};
				iemTargets->SetColorTargetBlendModes(1, &iemBlendModes);

				re::TextureTarget::TargetParams targetParams;
				targetParams.m_targetFace = face;
				targetParams.m_targetMip = 0;

				iemTargets->SetColorTarget(0, ambientProperties.m_ambient.m_IEMTex, targetParams);
				iemTargets->SetViewport(re::Viewport(0, 0, iemTexWidthHeight, iemTexWidthHeight));
				iemTargets->SetScissorRect(re::ScissorRect(0, 0, iemTexWidthHeight, iemTexWidthHeight));

				iemStage->SetTextureTargetSet(iemTargets);

				iemStage->SetStagePipelineState(iblStageParams);

				iemStage->AddBatch(cubeMeshBatch);

				pipeline.AppendSingleFrameRenderStage(std::move(iemStage));
			}
		}

		// 1st frame: Generate PMREM (Pre-filtered Mip-mapped Radiance Environment Map) cubemap for specular reflections
		{
			const uint32_t pmremTexWidthHeight =
				static_cast<uint32_t>(Config::Get()->GetValue<int>(en::ConfigKeys::k_pmremTexWidthHeight));

			// PMREM-specific texture params:
			Texture::TextureParams pmremTexParams;
			pmremTexParams.m_width = pmremTexWidthHeight;
			pmremTexParams.m_height = pmremTexWidthHeight;
			pmremTexParams.m_faces = 6;
			pmremTexParams.m_usage = Texture::Usage::ColorTarget;
			pmremTexParams.m_dimension = Texture::Dimension::TextureCubeMap;
			pmremTexParams.m_format = Texture::Format::RGBA16F;
			pmremTexParams.m_colorSpace = Texture::ColorSpace::Linear;
			pmremTexParams.m_addToSceneData = false;
			pmremTexParams.m_mipMode = re::Texture::MipMode::Allocate;

			shared_ptr<Shader> pmremShader = re::Shader::Create(en::ShaderNames::k_generatePMREMShaderName);

			const string PMREMTextureName = iblTexture->GetName() + "_PMREMTexture";
			ambientProperties.m_ambient.m_PMREMTex = re::Texture::Create(PMREMTextureName, pmremTexParams, false);

			const uint32_t totalMipLevels = ambientProperties.m_ambient.m_PMREMTex->GetNumMips();

			for (uint32_t face = 0; face < 6; face++)
			{
				for (uint32_t currentMipLevel = 0; currentMipLevel < totalMipLevels; currentMipLevel++)
				{
					std::string const& postFix = std::format("Face {}, Mip {}", face, currentMipLevel);
					std::string const& stageName = std::format("PMREM generation: {}", postFix);

					re::RenderStage::GraphicsStageParams gfxStageParams;
					std::shared_ptr<re::RenderStage> pmremStage = re::RenderStage::CreateSingleFrameGraphicsStage(
						stageName, gfxStageParams);

					pmremStage->SetStageShader(pmremShader);
					pmremStage->AddTextureInput(
						"Tex0",
						iblTexture,
						re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Clamp_LinearMipMapLinear_Linear));

					// Construct a camera param block to draw into our cubemap rendering targets:
					cubemapCamParams.g_view = cubemapViews[face];
					shared_ptr<re::ParameterBlock> pb = re::ParameterBlock::Create(
						gr::Camera::CameraParams::s_shaderName,
						cubemapCamParams,
						re::ParameterBlock::PBType::SingleFrame);
					pmremStage->AddSingleFrameParameterBlock(pb);

					IEMPMREMGenerationParams const& pmremGenerationParams = GetIEMPMREMGenerationParamsData(
							currentMipLevel, totalMipLevels, face, iblTexture->Width(), iblTexture->Height());
					shared_ptr<re::ParameterBlock> pmremGenerationPB = re::ParameterBlock::Create(
						IEMPMREMGenerationParams::s_shaderName,
						pmremGenerationParams,
						re::ParameterBlock::PBType::SingleFrame);
					pmremStage->AddSingleFrameParameterBlock(pmremGenerationPB);

					re::TextureTarget::TargetParams targetParams;
					targetParams.m_targetFace = face;
					targetParams.m_targetMip = currentMipLevel;

					std::shared_ptr<TextureTargetSet> pmremTargetSet =
						re::TextureTargetSet::Create("PMREM texture targets: Face " + postFix);

					pmremTargetSet->SetColorTarget(0, ambientProperties.m_ambient.m_PMREMTex, targetParams);

					const glm::vec4 mipDimensions = 
						ambientProperties.m_ambient.m_PMREMTex->GetSubresourceDimensions(currentMipLevel);
					const uint32_t mipWidth = static_cast<uint32_t>(mipDimensions.x);
					const uint32_t mipHeight = static_cast<uint32_t>(mipDimensions.y);

					pmremTargetSet->SetViewport(re::Viewport(0, 0, mipWidth, mipHeight));
					pmremTargetSet->SetScissorRect(re::ScissorRect(0, 0, mipWidth, mipHeight));

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
		lightTargetParams.m_mipMode = re::Texture::MipMode::None;
		lightTargetParams.m_addToSceneData = false;
		lightTargetParams.m_clear.m_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);

		std::shared_ptr<Texture> outputTexture = re::Texture::Create("DeferredLightTarget", lightTargetParams, false);

		re::TextureTarget::TargetParams colorTargetParams;

		std::shared_ptr<TextureTargetSet> deferredLightingTargetSet = 
			re::TextureTargetSet::Create("Deferred light targets");
		deferredLightingTargetSet->SetColorTarget(0, outputTexture, colorTargetParams);

		Texture::TextureParams lightDepthTargetParams(lightTargetParams);
		lightDepthTargetParams.m_usage = Texture::Usage::DepthTarget;

		// We need the depth buffer attached, but with depth writes disabled:
		re::TextureTarget::TargetParams depthTargetParams;
		depthTargetParams.m_channelWriteMode.R = re::TextureTarget::TargetParams::ChannelWrite::Disabled;

		deferredLightingTargetSet->SetDepthStencilTarget(
			gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(), 
			depthTargetParams);
		
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

		re::PipelineState ambientStageParams;
		ambientStageParams.SetClearTarget(re::PipelineState::ClearTarget::Color);

		// Ambient/directional lights use back face culling, as they're fullscreen quads
		ambientStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back); 

		// Our fullscreen quad is on the far plane; We only want to light something if the quad is behind the geo (i.e.
		// the quad's depth is greater than what is in the depth buffer)
		ambientStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::Greater);
		
		// Ambient light stage:
		m_ambientStage->SetStageShader(re::Shader::Create(en::ShaderNames::k_deferredAmbientLightShaderName));

		m_ambientStage->AddPermanentParameterBlock(deferredLightingCam->GetCameraParams());
		m_ambientStage->SetStagePipelineState(ambientStageParams);

		// Ambient PB:
		const uint32_t totalPMREMMipLevels = ambientProperties.m_ambient.m_PMREMTex->GetNumMips();
		
		const AmbientLightParams ambientLightParams = GetAmbientLightParamsData(
			totalPMREMMipLevels,
			static_cast<float>(ambientProperties.m_diffuseEnabled),
			static_cast<float>(ambientProperties.m_specularEnabled));

		m_ambientParams = re::ParameterBlock::Create(
			AmbientLightParams::s_shaderName,
			ambientLightParams,
			re::ParameterBlock::PBType::Mutable);

		m_ambientStage->AddPermanentParameterBlock(m_ambientParams);

		// If we made it this far, append the ambient stage:
		pipeline.AppendRenderStage(m_ambientStage);
		

		// Key light stage:
		shared_ptr<Light> keyLight = SceneManager::GetSceneData()->GetKeyLight();

		re::PipelineState keylightStageParams(ambientStageParams);
		if (keyLight)
		{
			if (!ambientIsValid) // Don't clear after 1st light
			{
				keylightStageParams.SetClearTarget(re::PipelineState::ClearTarget::Color);
			}
			else
			{
				keylightStageParams.SetClearTarget(re::PipelineState::ClearTarget::None);
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

			re::PipelineState pointlightStageParams(keylightStageParams);

			if (!keyLight && !ambientIsValid)
			{
				keylightStageParams.SetClearTarget(re::PipelineState::ClearTarget::Color);
			}

			// Pointlights only illuminate something if the sphere volume is behind it
			pointlightStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::GEqual);

			if (!iblTexture && !keyLight) // Don't clear after 1st light
			{
				pointlightStageParams.SetClearTarget(re::PipelineState::ClearTarget::Color);
			}
			else
			{
				pointlightStageParams.SetClearTarget(re::PipelineState::ClearTarget::None);
			}

			pointlightStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Front); // Cull front faces of light volumes
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
					Sampler::GetSampler(Sampler::WrapAndFilterMode::Wrap_Linear_Linear));
			}
			if (keyLight)
			{
				m_keylightStage->AddTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[slot],
					gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
					Sampler::GetSampler(Sampler::WrapAndFilterMode::Wrap_Linear_Linear));
			}
			if (!pointLights.empty())
			{
				m_pointlightStage->AddTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[slot],
					gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
					Sampler::GetSampler(Sampler::WrapAndFilterMode::Wrap_Linear_Linear));
			}
		}


		// Attach the GBUffer depth input:
		constexpr uint8_t depthBufferSlot = gr::GBufferGraphicsSystem::GBufferDepth;

		if (keyLight)
		{
			m_keylightStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
				gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
				Sampler::GetSampler(Sampler::WrapAndFilterMode::Wrap_Linear_Linear));

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
					Sampler::GetSampler(Sampler::WrapAndFilterMode::Wrap_Linear_Linear));
			}
		}

		if (!pointLights.empty())
		{
			m_pointlightStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
				gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
				Sampler::GetSampler(Sampler::WrapAndFilterMode::Wrap_Linear_Linear));
		}

		if (ambientIsValid)
		{
			m_ambientStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
				gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
				Sampler::GetSampler(Sampler::WrapAndFilterMode::Wrap_Linear_Linear));

			// Add IBL texture inputs for ambient stage:
			m_ambientStage->AddTextureInput(
				"CubeMap0",
				ambientProperties.m_ambient.m_IEMTex,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::Wrap_Linear_Linear)
			);

			m_ambientStage->AddTextureInput(
				"CubeMap1",
				ambientProperties.m_ambient.m_PMREMTex,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::Wrap_LinearMipMapLinear_Linear)
			);

			m_ambientStage->AddTextureInput(
				"Tex7",
				ambientProperties.m_ambient.m_BRDF_integrationMap,
				Sampler::GetSampler(Sampler::WrapAndFilterMode::Clamp_Nearest_Nearest)
			);
		}
	}


	void DeferredLightingGraphicsSystem::PreRender()
	{
		CreateBatches();

		gr::Light::LightTypeProperties& ambientProperties =
			en::SceneManager::GetSceneData()->GetAmbientLight()->AccessLightTypeProperties(Light::AmbientIBL);

		const uint32_t totalPMREMMipLevels = ambientProperties.m_ambient.m_PMREMTex->GetNumMips();

		const AmbientLightParams ambientLightParams = GetAmbientLightParamsData(
			totalPMREMMipLevels,
			static_cast<float>(ambientProperties.m_diffuseEnabled),
			static_cast<float>(ambientProperties.m_specularEnabled));

		m_ambientParams->Commit(ambientLightParams);
	}


	void DeferredLightingGraphicsSystem::CreateBatches()
	{
		// Ambient stage batches:
		const Batch ambientFullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);
		m_ambientStage->AddBatch(ambientFullscreenQuadBatch);

		// Keylight stage batches:
		shared_ptr<Light> const keyLight = SceneManager::GetSceneData()->GetKeyLight();
		if (keyLight)
		{
			Batch keylightFullscreenQuadBatch = Batch(m_screenAlignedQuad.get(), nullptr);

			LightParams const& keylightParams = GetLightParamData(keyLight, m_keylightStage->GetTextureTargetSet());
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
			LightParams const& pointlightParams = GetLightParamData(pointLights[i], m_pointlightStage->GetTextureTargetSet());
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
					std::const_pointer_cast<re::Sampler>(Sampler::GetSampler(Sampler::WrapAndFilterMode::Wrap_Linear_Linear));

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