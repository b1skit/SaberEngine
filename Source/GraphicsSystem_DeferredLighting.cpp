// © 2022 Adam Badke. All rights reserved.
#include "BatchManager.h"
#include "Config.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_Shadows.h"
#include "LightRenderData.h"
#include "MeshFactory.h"
#include "ParameterBlock.h"
#include "Sampler.h"
#include "SceneManager.h"
#include "ShadowMapRenderData.h"
#include "RenderManager.h"
#include "RenderStage.h"


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
			static_cast<uint32_t>(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_brdfLUTWidthHeight));

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

		const int numIEMSamples = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_iemNumSamples);
		const int numPMREMSamples = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_pmremNumSamples);

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
			static_cast<uint32_t>(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_brdfLUTWidthHeight));

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


	LightParams GetLightParamData(
		gr::Light::RenderData const& lightData, 
		gr::Transform::RenderData const& transformData,
		gr::ShadowMap::RenderData const* shadowData,
		gr::Camera::RenderData const* shadowCamData,
		std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		SEAssert("Ambient lights do not use the LightParams structure",
			lightData.m_lightType != fr::Light::LightType::AmbientIBL_Deferred);

		SEAssert("Shadow data and shadow camera data depend on each other", 
			(shadowData != nullptr) == (shadowCamData != nullptr));

		LightParams lightParams;
		memset(&lightParams, 0, sizeof(LightParams)); // Ensure unused elements are zeroed

		// Set type-specific params:
		bool hasShadow = false;
		switch (lightData.m_lightType)
		{
		case fr::Light::LightType::Directional_Deferred:
		{
			lightParams.g_lightColorIntensity = lightData.m_typeProperties.m_directional.m_colorIntensity;
			lightParams.g_lightWorldPosRadius = glm::vec4(transformData.m_globalForward, 0.f); // WorldPos == Light dir

			hasShadow = lightData.m_typeProperties.m_directional.m_hasShadow;
		}
		break;
		case fr::Light::LightType::Point_Deferred:
		{
			lightParams.g_lightColorIntensity = lightData.m_typeProperties.m_point.m_colorIntensity;
			
			lightParams.g_lightWorldPosRadius = 
				glm::vec4(transformData.m_globalPosition, lightData.m_typeProperties.m_point.m_emitterRadius);

			hasShadow = lightData.m_typeProperties.m_point.m_hasShadow;
		}
		break;
		default:
			SEAssertF("Light type does not use this param block");
		}

		if (hasShadow)
		{
			lightParams.g_shadowMapTexelSize = shadowData->m_textureDims;
			
			lightParams.g_shadowCamNearFarBiasMinMax = glm::vec4(
				shadowCamData->m_cameraConfig.m_near,
				shadowCamData->m_cameraConfig.m_far,
				shadowData->m_minMaxShadowBias);

			switch (lightData.m_lightType)
			{
			case fr::Light::LightType::Directional_Deferred:
			{
				lightParams.g_shadowCam_VP = shadowCamData->m_cameraParams.g_viewProjection;
			}
			break;
			case fr::Light::LightType::Point_Deferred:
			{
				lightParams.g_shadowCam_VP = glm::mat4(0.0f); // Unused by point light cube shadow maps
			}
			break;
			default:
				SEAssertF("Light shadow type does not use this param block");
			}
		}
		else
		{
			lightParams.g_shadowMapTexelSize = glm::vec4(0.f);
			lightParams.g_shadowCamNearFarBiasMinMax = glm::vec4(0.f);
			lightParams.g_shadowCam_VP = glm::mat4(0.0f);
		}

		lightParams.g_renderTargetResolution = targetSet->GetTargetDimensions();
		
		lightParams.g_intensityScale = glm::vec4(
			static_cast<float>(lightData.m_diffuseEnabled),
			static_cast<float>(lightData.m_specularEnabled),
			0.f,
			0.f);

		return lightParams;
	}
}


namespace gr
{
	constexpr char const* k_gsName = "Deferred Lighting Graphics System";


	DeferredLightingGraphicsSystem::DeferredLightingGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, NamedObject(k_gsName)
		, m_shadowGS(nullptr)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_ambientStage = re::RenderStage::CreateGraphicsStage("Ambient light stage", gfxStageParams);
		m_directionalStage = re::RenderStage::CreateGraphicsStage("Keylight stage", gfxStageParams);
		m_pointStage = re::RenderStage::CreateGraphicsStage("Pointlight stage", gfxStageParams);

		// Cube mesh, for rendering of IBL cubemaps
		m_cubeMeshPrimitive = gr::meshfactory::CreateCube();
	}


	void DeferredLightingGraphicsSystem::CreateResourceGenerationStages(re::StagePipeline& pipeline)
	{
		SEAssert("We currently expect render data for exactly 1 ambient light to exist",
			m_renderData[gr::Light::AmbientIBL_Deferred].size() == 1);
		gr::Light::RenderData const& ambientRenderData = m_renderData[gr::Light::AmbientIBL_Deferred][0];

		re::Texture const* iblTexture = ambientRenderData.m_typeProperties.m_ambient.m_iblTex;

		// 1st frame: Generate the pre-integrated BRDF LUT via a single-frame compute stage:
		{
			re::RenderStage::ComputeStageParams computeStageParams;
			std::shared_ptr<re::RenderStage> brdfStage =
				re::RenderStage::CreateSingleFrameComputeStage("BRDF pre-integration compute stage", computeStageParams);

			re::PipelineState brdfPipelineState;
			brdfPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
			brdfPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);

			brdfStage->SetStageShader(
				re::Shader::GetOrCreate(en::ShaderNames::k_generateBRDFIntegrationMapShaderName, brdfPipelineState));

			const uint32_t brdfTexWidthHeight = 
				static_cast<uint32_t>(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_brdfLUTWidthHeight));

			// Create a render target texture:			
			re::Texture::TextureParams brdfParams;
			brdfParams.m_width = brdfTexWidthHeight;
			brdfParams.m_height = brdfTexWidthHeight;
			brdfParams.m_faces = 1;
			brdfParams.m_usage = 
				static_cast<re::Texture::Usage>(re::Texture::Usage::ComputeTarget | re::Texture::Usage::Color);
			brdfParams.m_dimension = re::Texture::Dimension::Texture2D;
			brdfParams.m_format = re::Texture::Format::RGBA16F;
			brdfParams.m_colorSpace = re::Texture::ColorSpace::Linear;
			brdfParams.m_mipMode = re::Texture::MipMode::None;
			brdfParams.m_addToSceneData = false;
			brdfParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

			m_BRDF_integrationMap = re::Texture::Create("BRDFIntegrationMap", brdfParams, false);

			std::shared_ptr<re::TextureTargetSet> brdfStageTargets = re::TextureTargetSet::Create("BRDF Stage Targets");

			re::TextureTarget::TargetParams colorTargetParams;

			brdfStageTargets->SetColorTarget(0, m_BRDF_integrationMap, colorTargetParams);
			brdfStageTargets->SetViewport(re::Viewport(0, 0, brdfTexWidthHeight, brdfTexWidthHeight));
			brdfStageTargets->SetScissorRect(re::ScissorRect(0, 0, brdfTexWidthHeight, brdfTexWidthHeight));

			re::TextureTarget::TargetParams::BlendModes brdfBlendModes
			{
				re::TextureTarget::TargetParams::BlendMode::One,
				re::TextureTarget::TargetParams::BlendMode::Zero,
			};
			brdfStageTargets->SetColorTargetBlendModes(1, &brdfBlendModes);

			brdfStage->SetTextureTargetSet(brdfStageTargets);

			BRDFIntegrationParams const& brdfIntegrationParams = GetBRDFIntegrationParamsData();
			std::shared_ptr<re::ParameterBlock> brdfIntegrationPB = re::ParameterBlock::Create(
				BRDFIntegrationParams::s_shaderName,
				brdfIntegrationParams,
				re::ParameterBlock::PBType::SingleFrame);
			brdfStage->AddSingleFrameParameterBlock(brdfIntegrationPB);

			// Add our dispatch information to a compute batch. Note: We use numthreads = (1,1,1)
			re::Batch computeBatch = re::Batch(re::Batch::Lifetime::SingleFrame, re::Batch::ComputeParams{
				.m_threadGroupCount = glm::uvec3(brdfTexWidthHeight, brdfTexWidthHeight, 1u) });

			brdfStage->AddBatch(computeBatch);

			pipeline.AppendSingleFrameRenderStage(std::move(brdfStage));
		}

		// Common IBL texture generation stage params:
		re::PipelineState iblStageParams;
		iblStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
		iblStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);

		// Camera render params for 6 cubemap faces; Just need to update g_view for each face/stage
		gr::Camera::CameraParams cubemapCamParams{};

		cubemapCamParams.g_projection = gr::Camera::BuildPerspectiveProjectionMatrix(
			glm::radians(90.f), // yFOV
			1.f,				// Aspect ratio
			0.1f,				// Near
			10.f);				// Far

		cubemapCamParams.g_viewProjection = glm::mat4(1.f); // Identity; unused
		cubemapCamParams.g_invViewProjection = glm::mat4(1.f); // Identity; unused
		cubemapCamParams.g_cameraWPos = glm::vec4(0.f, 0.f, 0.f, 0.f); // Unused

		std::vector<glm::mat4> const& cubemapViews = gr::Camera::BuildCubeViewMatrices(glm::vec3(0.f));

		// Create a cube mesh batch, for reuse during the initial frame IBL rendering:
		re::Batch cubeMeshBatch = re::Batch(re::Batch::Lifetime::SingleFrame, m_cubeMeshPrimitive.get(), nullptr);

		// TODO: We should use equirectangular images, instead of bothering to convert to cubemaps for IEM/PMREM
		// -> Need to change the HLSL Get___DominantDir functions to ensure the result is normalized


		// 1st frame: Generate an IEM (Irradiance Environment Map) cubemap texture for diffuse irradiance
		{
			const uint32_t iemTexWidthHeight =
				static_cast<uint32_t>(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_iemTexWidthHeight));

			// IEM-specific texture params:
			re::Texture::TextureParams iemTexParams;
			iemTexParams.m_width = iemTexWidthHeight;
			iemTexParams.m_height = iemTexWidthHeight;
			iemTexParams.m_faces = 6;
			iemTexParams.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::Color);
			iemTexParams.m_dimension = re::Texture::Dimension::TextureCubeMap;
			iemTexParams.m_format = re::Texture::Format::RGBA16F;
			iemTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
			iemTexParams.m_addToSceneData = false;
			iemTexParams.m_mipMode = re::Texture::MipMode::None;

			std::shared_ptr<re::Shader> iemShader = 
				re::Shader::GetOrCreate(en::ShaderNames::k_generateIEMShaderName, iblStageParams);

			const std::string IEMTextureName = iblTexture->GetName() + "_IEMTexture";
			m_IEMTex = re::Texture::Create(IEMTextureName, iemTexParams, false);

			for (uint32_t face = 0; face < 6; face++)
			{
				re::RenderStage::GraphicsStageParams gfxStageParams;
				std::shared_ptr<re::RenderStage> iemStage = re::RenderStage::CreateSingleFrameGraphicsStage(
					"IEM generation: Face " + std::to_string(face + 1) + "/6", gfxStageParams);

				iemStage->SetStageShader(iemShader);
				iemStage->AddTextureInput(
					"Tex0",
					iblTexture,
					re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Clamp_LinearMipMapLinear_Linear));

				IEMPMREMGenerationParams const& iemGenerationParams = 
					GetIEMPMREMGenerationParamsData(0, 1, face, iblTexture->Width(), iblTexture->Height());
				std::shared_ptr<re::ParameterBlock> iemGenerationPB = re::ParameterBlock::Create(
					IEMPMREMGenerationParams::s_shaderName,
					iemGenerationParams,
					re::ParameterBlock::PBType::SingleFrame);
				iemStage->AddSingleFrameParameterBlock(iemGenerationPB);

				// Construct a camera param block to draw into our cubemap rendering targets:
				cubemapCamParams.g_view = cubemapViews[face];
				std::shared_ptr<re::ParameterBlock> cubemapCamParamsPB = re::ParameterBlock::Create(
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

				iemTargets->SetColorTarget(0, m_IEMTex, targetParams);
				iemTargets->SetViewport(re::Viewport(0, 0, iemTexWidthHeight, iemTexWidthHeight));
				iemTargets->SetScissorRect(re::ScissorRect(0, 0, iemTexWidthHeight, iemTexWidthHeight));

				iemStage->SetTextureTargetSet(iemTargets);

				iemStage->AddBatch(cubeMeshBatch);

				pipeline.AppendSingleFrameRenderStage(std::move(iemStage));
			}
		}

		// 1st frame: Generate PMREM (Pre-filtered Mip-mapped Radiance Environment Map) cubemap for specular reflections
		{
			const uint32_t pmremTexWidthHeight =
				static_cast<uint32_t>(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_pmremTexWidthHeight));

			// PMREM-specific texture params:
			re::Texture::TextureParams pmremTexParams;
			pmremTexParams.m_width = pmremTexWidthHeight;
			pmremTexParams.m_height = pmremTexWidthHeight;
			pmremTexParams.m_faces = 6;
			pmremTexParams.m_usage = static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::Color);
			pmremTexParams.m_dimension = re::Texture::Dimension::TextureCubeMap;
			pmremTexParams.m_format = re::Texture::Format::RGBA16F;
			pmremTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
			pmremTexParams.m_addToSceneData = false;
			pmremTexParams.m_mipMode = re::Texture::MipMode::Allocate;

			std::shared_ptr<re::Shader> pmremShader =
				re::Shader::GetOrCreate(en::ShaderNames::k_generatePMREMShaderName, iblStageParams);

			const std::string PMREMTextureName = iblTexture->GetName() + "_PMREMTexture";
			m_PMREMTex = re::Texture::Create(PMREMTextureName, pmremTexParams, false);

			const uint32_t totalMipLevels = m_PMREMTex->GetNumMips();

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
					std::shared_ptr<re::ParameterBlock> pb = re::ParameterBlock::Create(
						gr::Camera::CameraParams::s_shaderName,
						cubemapCamParams,
						re::ParameterBlock::PBType::SingleFrame);
					pmremStage->AddSingleFrameParameterBlock(pb);

					IEMPMREMGenerationParams const& pmremGenerationParams = GetIEMPMREMGenerationParamsData(
							currentMipLevel, totalMipLevels, face, iblTexture->Width(), iblTexture->Height());
					std::shared_ptr<re::ParameterBlock> pmremGenerationPB = re::ParameterBlock::Create(
						IEMPMREMGenerationParams::s_shaderName,
						pmremGenerationParams,
						re::ParameterBlock::PBType::SingleFrame);
					pmremStage->AddSingleFrameParameterBlock(pmremGenerationPB);

					re::TextureTarget::TargetParams targetParams;
					targetParams.m_targetFace = face;
					targetParams.m_targetMip = currentMipLevel;

					std::shared_ptr<re::TextureTargetSet> pmremTargetSet =
						re::TextureTargetSet::Create("PMREM texture targets: Face " + postFix);

					pmremTargetSet->SetColorTarget(0, m_PMREMTex, targetParams);

					const glm::vec4 mipDimensions = 
						m_PMREMTex->GetSubresourceDimensions(currentMipLevel);
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

					pmremStage->AddBatch(cubeMeshBatch);

					pipeline.AppendSingleFrameRenderStage(std::move(pmremStage));
				}
			}
		}
	}


	void DeferredLightingGraphicsSystem::Create(re::RenderSystem& renderSystem, re::StagePipeline& pipeline)
	{
		m_shadowGS = m_owningGraphicsSystemManager->GetGraphicsSystem<gr::ShadowsGraphicsSystem>();
		SEAssert("Shadow graphics system not found", m_shadowGS != nullptr);

		GBufferGraphicsSystem* gBufferGS = 
			m_owningGraphicsSystemManager->GetGraphicsSystem<GBufferGraphicsSystem>();
		SEAssert("GBuffer GS not found", gBufferGS != nullptr);
		
		// Create a shared lighting stage texture target:
		re::Texture::TextureParams lightTargetTexParams;
		lightTargetTexParams.m_width = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey);
		lightTargetTexParams.m_height = en::Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey);
		lightTargetTexParams.m_faces = 1;
		lightTargetTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::Color);
		lightTargetTexParams.m_dimension = re::Texture::Dimension::Texture2D;
		lightTargetTexParams.m_format = re::Texture::Format::RGBA16F;
		lightTargetTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		lightTargetTexParams.m_mipMode = re::Texture::MipMode::None;
		lightTargetTexParams.m_addToSceneData = false;
		lightTargetTexParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		std::shared_ptr<re::Texture> outputTexture = 
			re::Texture::Create("DeferredLightTarget", lightTargetTexParams, false);

		re::TextureTarget::TargetParams ambientTargetParams{};
		ambientTargetParams.m_clearMode = re::TextureTarget::TargetParams::ClearMode::Enabled;
		std::shared_ptr<re::TextureTargetSet> ambientTargetSet = re::TextureTargetSet::Create("Ambient light targets");
		ambientTargetSet->SetColorTarget(0, outputTexture, ambientTargetParams);

		// We need the depth buffer attached, but with depth writes disabled:
		re::TextureTarget::TargetParams depthTargetParams;
		depthTargetParams.m_channelWriteMode.R = re::TextureTarget::TargetParams::ChannelWrite::Disabled;

		ambientTargetSet->SetDepthStencilTarget(
			gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
			depthTargetParams);
		
		// All deferred lighting is additive
		re::TextureTarget::TargetParams::BlendModes deferredBlendModes
		{
			re::TextureTarget::TargetParams::BlendMode::One,
			re::TextureTarget::TargetParams::BlendMode::One,
		};
		ambientTargetSet->SetColorTargetBlendModes(1, &deferredBlendModes);

		// Set the target sets, even if the stages aren't actually used (to ensure they're still valid)
		m_ambientStage->SetTextureTargetSet(ambientTargetSet);

		// We'll be creating the data we need to render the scene's ambient light:
		const bool ambientIsValid = m_BRDF_integrationMap && m_IEMTex && m_PMREMTex; // ECS_CONVERSION: REMOVE THIS LOGIC: THIS SHOULD ALWAYS BE TRUE!!!

		re::PipelineState ambientStageParams;

		// Ambient/directional lights use back face culling, as they're fullscreen quads
		ambientStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back); 

		// Our fullscreen quad is on the far plane; We only want to light something if the quad is behind the geo (i.e.
		// the quad's depth is greater than what is in the depth buffer)
		ambientStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::Greater);
		
		// Ambient light stage:
		m_ambientStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_deferredAmbientLightShaderName, ambientStageParams));

		m_ambientStage->AddPermanentParameterBlock(m_owningGraphicsSystemManager->GetActiveCameraParams());

		// Ambient PB:
		const uint32_t totalPMREMMipLevels = m_PMREMTex->GetNumMips();
		
		SEAssert("We currently expect render data for exactly 1 ambient light to exist",
			m_renderData[gr::Light::AmbientIBL_Deferred].size() == 1);
		gr::Light::RenderData const& ambientRenderData = m_renderData[gr::Light::AmbientIBL_Deferred][0];

		const AmbientLightParams ambientLightParams = GetAmbientLightParamsData(
			totalPMREMMipLevels,
			static_cast<float>(ambientRenderData.m_diffuseEnabled),
			static_cast<float>(ambientRenderData.m_specularEnabled));

		m_ambientParams = re::ParameterBlock::Create(
			AmbientLightParams::s_shaderName,
			ambientLightParams,
			re::ParameterBlock::PBType::Mutable);

		m_ambientStage->AddPermanentParameterBlock(m_ambientParams);

		// If we made it this far, append the ambient stage:
		pipeline.AppendRenderStage(m_ambientStage);
		

		re::PipelineState directionalStageParams(ambientStageParams);

		// Key light stage:
		const bool hasDirectionalLight = !m_renderData[gr::Light::LightType::Directional_Deferred].empty();
		if (hasDirectionalLight)
		{
			SEAssert("We currently assume there will only be 1 directional light (even though it's not necessary to)", 
				m_renderData[gr::Light::LightType::Directional_Deferred].size() == 1);

			re::TextureTarget::TargetParams directionalTargetParams;
			directionalTargetParams.m_clearMode = re::TextureTarget::TargetParams::ClearMode::Disabled;

			if (!ambientIsValid) // Don't clear after 1st light
			{
				directionalTargetParams.m_clearMode = re::TextureTarget::TargetParams::ClearMode::Enabled;
			}
			else
			{
				directionalTargetParams.m_clearMode = re::TextureTarget::TargetParams::ClearMode::Disabled;
			}

			std::shared_ptr<re::TextureTargetSet> directionalTargetSet = 
				re::TextureTargetSet::Create("Directional light targets");

			directionalTargetSet->SetColorTarget(0, outputTexture, directionalTargetParams);

			directionalTargetSet->SetDepthStencilTarget(
				gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
				depthTargetParams);

			directionalTargetSet->SetColorTargetBlendModes(1, &deferredBlendModes);

			m_directionalStage->SetTextureTargetSet(directionalTargetSet);

			m_directionalStage->SetStageShader(
				re::Shader::GetOrCreate(en::ShaderNames::k_deferredDirectionalLightShaderName, directionalStageParams));

			m_directionalStage->AddPermanentParameterBlock(m_owningGraphicsSystemManager->GetActiveCameraParams());

			pipeline.AppendRenderStage(m_directionalStage);
		}


		// Point light stage:
		const bool hasPointLights = !m_renderData[fr::Light::LightType::Point_Deferred].empty();
		if (hasPointLights)
		{
			m_pointStage->AddPermanentParameterBlock(m_owningGraphicsSystemManager->GetActiveCameraParams());

			re::TextureTarget::TargetParams pointTargetParams;
			
			if (!hasDirectionalLight && !ambientIsValid)
			{
				pointTargetParams.m_clearMode = re::TextureTarget::TargetParams::ClearMode::Enabled;
			}
			else
			{
				pointTargetParams.m_clearMode = re::TextureTarget::TargetParams::ClearMode::Disabled;
			}

			std::shared_ptr<re::TextureTargetSet> pointTargetSet = 
				re::TextureTargetSet::Create("Point light targets");

			pointTargetSet->SetColorTarget(0, outputTexture, pointTargetParams);

			pointTargetSet->SetDepthStencilTarget(
				gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
				depthTargetParams);

			pointTargetSet->SetColorTargetBlendModes(1, &deferredBlendModes);

			m_pointStage->SetTextureTargetSet(pointTargetSet);

			re::PipelineState pointStageParams(directionalStageParams);

			// Pointlights only illuminate something if the sphere volume is behind it
			pointStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::GEqual);

			pointStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Front); // Cull front faces of light volumes

			m_pointStage->SetStageShader(
				re::Shader::GetOrCreate(en::ShaderNames::k_deferredPointLightShaderName, pointStageParams));

			pipeline.AppendRenderStage(m_pointStage);
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
					re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear));
			}
			if (hasDirectionalLight)
			{
				m_directionalStage->AddTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[slot],
					gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
					re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear));
			}
			if (hasPointLights)
			{
				m_pointStage->AddTextureInput(
					GBufferGraphicsSystem::GBufferTexNames[slot],
					gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
					re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear));
			}
		}


		// Attach the GBUffer depth input:
		constexpr uint8_t depthBufferSlot = gr::GBufferGraphicsSystem::GBufferDepth;

		if (hasDirectionalLight)
		{
			m_directionalStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
				gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
				re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear));

			SEAssert("We currently assume there will only be 1 directional light (even though it's not necessary to)",
				m_renderData[gr::Light::LightType::Directional_Deferred].size() == 1);

			gr::Light::RenderData const& directionalLightData = 
				m_renderData[gr::Light::LightType::Directional_Deferred][0];
			
			// Directional shadow map:
			if (directionalLightData.m_typeProperties.m_directional.m_hasShadow)
			{
				re::Texture const* directionalShadow = 
					m_shadowGS->GetShadowMap(gr::Light::LightType::Directional_Deferred, directionalLightData.m_lightID);

				// Set the key light shadow map:
				m_directionalStage->AddTextureInput(
					"Depth0",
					directionalShadow,
					re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear));
			}
		}

		if (hasPointLights)
		{
			m_pointStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
				gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
				re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear));
		}

		if (ambientIsValid)
		{
			m_ambientStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
				gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
				re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear));

			// Add IBL texture inputs for ambient stage:
			m_ambientStage->AddTextureInput(
				"CubeMap0",
				m_IEMTex,
				re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear));

			m_ambientStage->AddTextureInput(
				"CubeMap1",
				m_PMREMTex,
				re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_LinearMipMapLinear_Linear));

			m_ambientStage->AddTextureInput(
				"Tex7",
				m_BRDF_integrationMap,
				re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Clamp_Nearest_Nearest));
		}
	}


	void DeferredLightingGraphicsSystem::PreRender()
	{
		CreateBatches();

		const uint32_t totalPMREMMipLevels = m_PMREMTex->GetNumMips();

		SEAssert("We currently expect render data for exactly 1 ambient light to exist",
			m_renderData[gr::Light::AmbientIBL_Deferred].size() == 1);
		gr::Light::RenderData const& ambientRenderData = m_renderData[gr::Light::AmbientIBL_Deferred][0];

		const AmbientLightParams ambientLightParams = GetAmbientLightParamsData(
			totalPMREMMipLevels,
			static_cast<float>(ambientRenderData.m_diffuseEnabled),
			static_cast<float>(ambientRenderData.m_specularEnabled));

		m_ambientParams->Commit(ambientLightParams);
	}


	void DeferredLightingGraphicsSystem::CreateBatches()
	{
		gr::RenderDataManager const& renderDataMgr = m_owningGraphicsSystemManager->GetRenderData();

		// Ambient stage batches:
		SEAssert("We currently expect render data for exactly 1 ambient light to exist",
			m_renderData[gr::Light::AmbientIBL_Deferred].size() == 1);
		gr::Light::RenderData const& ambientRenderData = m_renderData[gr::Light::AmbientIBL_Deferred][0];

		gr::MeshPrimitive::RenderData const& ambientMeshPrimData = 
			renderDataMgr.GetObjectData<gr::MeshPrimitive::RenderData>(ambientRenderData.m_renderDataID);

		const re::Batch ambientFullscreenQuadBatch = 
			re::Batch(re::Batch::Lifetime::SingleFrame, ambientMeshPrimData, nullptr);

		m_ambientStage->AddBatch(ambientFullscreenQuadBatch);

		// Directional stage batches:
		const bool hasDirectionalLight = !m_renderData[gr::Light::LightType::Directional_Deferred].empty();
		if (hasDirectionalLight)
		{
			SEAssert("We currently assume there will only be 1 directional light (even though it's not necessary to)",
				m_renderData[gr::Light::LightType::Directional_Deferred].size() == 1);

			for (gr::Light::RenderData const& lightData : m_renderData[gr::Light::Directional_Deferred])
			{
				gr::Transform::RenderData const& directionalTransformData =
					renderDataMgr.GetTransformData(lightData.m_transformID);

				gr::ShadowMap::RenderData const* shadowData = nullptr;
				gr::Camera::RenderData const* shadowCamData = nullptr;
				if (lightData.m_typeProperties.m_directional.m_hasShadow)
				{
					shadowData = &m_shadowGS->GetShadowRenderData(gr::Light::Directional_Deferred, lightData.m_lightID);
					shadowCamData = &renderDataMgr.GetObjectData<gr::Camera::RenderData>(lightData.m_renderDataID);
				}

				LightParams const& directionalParams = GetLightParamData(
					lightData,
					directionalTransformData,
					shadowData,
					shadowCamData,
					m_directionalStage->GetTextureTargetSet());

				std::shared_ptr<re::ParameterBlock> directionalPB = re::ParameterBlock::Create(
					LightParams::s_shaderName,
					directionalParams,
					re::ParameterBlock::PBType::SingleFrame);

				gr::MeshPrimitive::RenderData const& meshData =
					renderDataMgr.GetObjectData<gr::MeshPrimitive::RenderData>(lightData.m_renderDataID);

				re::Batch directionalBatch =
					re::Batch(re::Batch::Lifetime::SingleFrame, meshData, nullptr);

				directionalBatch.SetParameterBlock(directionalPB);

				m_directionalStage->AddBatch(directionalBatch);
			}
		}

		// Pointlight stage batches:
		const bool hasPointLights = !m_renderData[fr::Light::LightType::Point_Deferred].empty();
		if (hasPointLights)
		{
			for(gr::Light::RenderData const& lightData : m_renderData[gr::Light::Point_Deferred])
			{				
				gr::MeshPrimitive::RenderData const& meshData = 
					renderDataMgr.GetObjectData<gr::MeshPrimitive::RenderData>(lightData.m_renderDataID);

				re::Batch pointlightBatch = re::Batch(re::Batch::Lifetime::SingleFrame, meshData, nullptr);

				// Point light params:
				gr::Transform::RenderData const& transformData =
					renderDataMgr.GetTransformData(lightData.m_transformID);
				
				pointlightBatch.SetParameterBlock(gr::Transform::CreateInstancedTransformParams(transformData));
				
				gr::ShadowMap::RenderData const* shadowData = nullptr;
				gr::Camera::RenderData const* shadowCamData = nullptr;
				const bool hasShadow = lightData.m_typeProperties.m_point.m_hasShadow;
				if (hasShadow)
				{
					shadowData = &m_shadowGS->GetShadowRenderData(gr::Light::Point_Deferred, lightData.m_lightID);
					shadowCamData =	&renderDataMgr.GetObjectData<gr::Camera::RenderData>(lightData.m_renderDataID);
				}

				LightParams const& pointLightParams = GetLightParamData(
					lightData, 
					transformData,
					shadowData,
					shadowCamData,
					m_pointStage->GetTextureTargetSet());

				std::shared_ptr<re::ParameterBlock> pointlightPB = re::ParameterBlock::Create(
					LightParams::s_shaderName,
					pointLightParams,
					re::ParameterBlock::PBType::SingleFrame);
				pointlightBatch.SetParameterBlock(pointlightPB);

				if (hasShadow) // Add the shadow map texture to the batch
				{
					re::Texture const* shadowMap = m_shadowGS->GetShadowMap(gr::Light::Point_Deferred, lightData.m_lightID);

					std::shared_ptr<re::Sampler> sampler =
						re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear);

					pointlightBatch.AddTextureAndSamplerInput("CubeMap0", shadowMap, sampler);
				}

				// Finally, add the completed batch:
				m_pointStage->AddBatch(pointlightBatch);
			}
		}
	}


	std::shared_ptr<re::TextureTargetSet const> DeferredLightingGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_ambientStage->GetTextureTargetSet();
	}
}