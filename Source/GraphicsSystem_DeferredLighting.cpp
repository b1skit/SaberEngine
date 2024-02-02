// © 2022 Adam Badke. All rights reserved.
#include "BatchManager.h"
#include "Config.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_XeGTAO.h"
#include "LightRenderData.h"
#include "MeshFactory.h"
#include "ParameterBlock.h"
#include "Sampler.h"
#include "SceneManager.h"
#include "ShadowMapRenderData.h"
#include "RenderManager.h"
#include "RenderStage.h"
#include "Shader.h"


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

		SEAssert(currentMipLevel >= 0 && numMipLevels >= 1,
			"Mip level params are invalid. These must be reasonable, even if they're not used (i.e. IEM generation)");

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
		glm::vec4 g_ssaoTexDims; // .xyzw = width, height, 1/width, 1/height

		static constexpr char const* const s_shaderName = "AmbientLightParams"; // Not counted towards size of struct
	};


	AmbientLightParams GetAmbientLightParamsData(
		uint32_t numPMREMMips, float diffuseScale, float specularScale, re::Texture const* ssaoTex)
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

		ambientLightParams.g_ssaoTexDims = glm::vec4(0.f);
		if (ssaoTex)
		{
			ambientLightParams.g_ssaoTexDims = ssaoTex->GetTextureDimenions();
		}

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
		void const* lightRenderData,
		gr::Light::Type lightType,
		gr::Transform::RenderData const& transformData,
		gr::ShadowMap::RenderData const* shadowData,
		gr::Camera::RenderData const* shadowCamData,
		std::shared_ptr<re::TextureTargetSet const> targetSet)
	{
		SEAssert(lightType != fr::Light::Type::AmbientIBL,
			"Ambient lights do not use the LightParams structure");

		SEAssert((shadowData != nullptr) == (shadowCamData != nullptr),
			"Shadow data and shadow camera data depend on each other");

		LightParams lightParams;
		memset(&lightParams, 0, sizeof(LightParams)); // Ensure unused elements are zeroed

		// Set type-specific params:
		bool hasShadow = false;
		bool diffuseEnabled = false;
		bool specEnabled = false;
		switch (lightType)
		{
		case fr::Light::Type::Directional:
		{
			gr::Light::RenderDataDirectional const* directionalData = 
				static_cast<gr::Light::RenderDataDirectional const*>(lightRenderData);
			
			lightParams.g_lightColorIntensity = directionalData->m_colorIntensity;
			lightParams.g_lightWorldPosRadius = glm::vec4(transformData.m_globalForward, 0.f); // WorldPos == Light dir

			hasShadow = directionalData->m_hasShadow;
			diffuseEnabled = directionalData->m_diffuseEnabled;
			specEnabled = directionalData->m_specularEnabled;
		}
		break;
		case fr::Light::Type::Point:
		{
			gr::Light::RenderDataPoint const* pointData =
				static_cast<gr::Light::RenderDataPoint const*>(lightRenderData);
			
			lightParams.g_lightColorIntensity = pointData->m_colorIntensity;
			
			lightParams.g_lightWorldPosRadius = 
				glm::vec4(transformData.m_globalPosition, pointData->m_emitterRadius);

			hasShadow = pointData->m_hasShadow;
			diffuseEnabled = pointData->m_diffuseEnabled;
			specEnabled = pointData->m_specularEnabled;
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

			switch (lightType)
			{
			case fr::Light::Type::Directional:
			{
				lightParams.g_shadowCam_VP = shadowCamData->m_cameraParams.g_viewProjection;
			}
			break;
			case fr::Light::Type::Point:
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
			static_cast<float>(diffuseEnabled),
			static_cast<float>(specEnabled),
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
	}


	void DeferredLightingGraphicsSystem::CreateResourceGenerationStages(re::StagePipeline& pipeline)
	{
		// Cube mesh, for rendering of IBL cubemaps
		if (m_cubeMeshPrimitive == nullptr)
		{
			m_cubeMeshPrimitive = gr::meshfactory::CreateCube();
		}

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		SEAssert(renderData.GetNumElementsOfType<gr::Light::RenderDataAmbientIBL>() == 1,
			"We currently expect render data for exactly 1 ambient light to exist");

		gr::RenderDataID ambientID = renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataAmbientIBL>()[0];

		gr::Light::RenderDataAmbientIBL const& ambientRenderData = 
			renderData.GetObjectData<gr::Light::RenderDataAmbientIBL>(ambientID);

		re::Texture const* iblTexture = ambientRenderData.m_iblTex;

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

			m_BRDF_integrationMap = re::Texture::Create("BRDFIntegrationMap", brdfParams);

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

		std::vector<glm::mat4> const& cubemapViews = gr::Camera::BuildAxisAlignedCubeViewMatrices(glm::vec3(0.f));

		// Create a cube mesh batch, for reuse during the initial frame IBL rendering:
		re::Batch cubeMeshBatch = re::Batch(re::Batch::Lifetime::SingleFrame, m_cubeMeshPrimitive.get());

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
			m_IEMTex = re::Texture::Create(IEMTextureName, iemTexParams);

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
			m_PMREMTex = re::Texture::Create(PMREMTextureName, pmremTexParams);

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
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_ambientStage = re::RenderStage::CreateGraphicsStage("Ambient light stage", gfxStageParams);
		m_directionalStage = re::RenderStage::CreateGraphicsStage("Keylight stage", gfxStageParams);
		m_pointStage = re::RenderStage::CreateGraphicsStage("Pointlight stage", gfxStageParams);


		m_shadowGS = m_graphicsSystemManager->GetGraphicsSystem<gr::ShadowsGraphicsSystem>();
		SEAssert(m_shadowGS != nullptr, "Shadow graphics system not found");

		GBufferGraphicsSystem* gBufferGS = 
			m_graphicsSystemManager->GetGraphicsSystem<GBufferGraphicsSystem>();
		SEAssert(gBufferGS != nullptr, "GBuffer GS not found");
		
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

		std::shared_ptr<re::Texture> outputTexture = re::Texture::Create("DeferredLightTarget", lightTargetTexParams);

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
		const bool ambientIsValid = m_BRDF_integrationMap && m_IEMTex && m_PMREMTex;

		re::PipelineState ambientStageParams;

		// Ambient/directional lights use back face culling, as they're fullscreen quads
		ambientStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back); 

		// Our fullscreen quad is on the far plane; We only want to light something if the quad is behind the geo (i.e.
		// the quad's depth is greater than what is in the depth buffer)
		ambientStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::Greater);
		
		// Ambient light stage:
		m_ambientStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_deferredAmbientLightShaderName, ambientStageParams));

		m_ambientStage->AddPermanentParameterBlock(m_graphicsSystemManager->GetActiveCameraParams());

		// Ambient PB:
		const uint32_t totalPMREMMipLevels = m_PMREMTex->GetNumMips();
		
		// Get/set the AO texture, if it exists:
		m_AOGS = m_graphicsSystemManager->GetGraphicsSystem<XeGTAOGraphicsSystem>();
		re::Texture const* ssaoTex = nullptr;
		if (m_AOGS)
		{
			m_ambientStage->AddTextureInput(
				"SSAOTex",
				m_AOGS->GetFinalTextureTargetSet()->GetColorTarget(0).GetTexture(),
				re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Clamp_Nearest_Nearest));

			ssaoTex = m_AOGS->GetFinalTextureTargetSet()->GetColorTarget(0).GetTexture().get();
		}

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		SEAssert(renderData.GetNumElementsOfType<gr::Light::RenderDataAmbientIBL>() == 1,
			"We currently expect render data for exactly 1 ambient light to exist");

		gr::RenderDataID ambientID = renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataAmbientIBL>()[0];
		gr::Light::RenderDataAmbientIBL const& ambientRenderData =
			renderData.GetObjectData<gr::Light::RenderDataAmbientIBL>(ambientID);

		const AmbientLightParams ambientLightParams = GetAmbientLightParamsData(
			totalPMREMMipLevels,
			static_cast<float>(ambientRenderData.m_diffuseEnabled),
			static_cast<float>(ambientRenderData.m_specularEnabled),
			ssaoTex);

		m_ambientParams = re::ParameterBlock::Create(
			AmbientLightParams::s_shaderName,
			ambientLightParams,
			re::ParameterBlock::PBType::Mutable);

		m_ambientStage->AddPermanentParameterBlock(m_ambientParams);


		// If we made it this far, append the ambient stage:
		pipeline.AppendRenderStage(m_ambientStage);
		

		re::PipelineState directionalStageParams(ambientStageParams);

		// Key light stage:
		const bool hasDirectionalLight = renderData.GetNumElementsOfType<gr::Light::RenderDataDirectional>() > 0;
		if (hasDirectionalLight)
		{
			SEAssert(renderData.GetNumElementsOfType<gr::Light::RenderDataDirectional>() == 1,
				"We currently assume there will only be 1 directional light (even though it's not necessary to)");

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

			m_directionalStage->AddPermanentParameterBlock(m_graphicsSystemManager->GetActiveCameraParams());

			pipeline.AppendRenderStage(m_directionalStage);
		}


		// Point light stage:
		const bool hasPointLights = renderData.GetNumElementsOfType<gr::Light::RenderDataPoint>() > 0;
		if (hasPointLights)
		{
			m_pointStage->AddPermanentParameterBlock(m_graphicsSystemManager->GetActiveCameraParams());

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

			SEAssert(renderData.GetNumElementsOfType<gr::Light::RenderDataDirectional>() == 1,
				"We currently assume there will only be 1 directional light (even though it's not necessary to)");

			std::vector<gr::RenderDataID> const& directionalIDs = 
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataDirectional>();

			auto directionalItr = renderData.IDBegin(directionalIDs);

			gr::Light::RenderDataDirectional const& directionalLightData = 
				directionalItr.Get<gr::Light::RenderDataDirectional>();

			// Directional shadow map:
			if (directionalLightData.m_hasShadow)
			{
				re::Texture const* directionalShadow =
					m_shadowGS->GetShadowMap(gr::Light::Type::Directional, directionalLightData.m_renderDataID);

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

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		SEAssert(renderData.GetNumElementsOfType<gr::Light::RenderDataAmbientIBL>() == 1,
			"We currently expect render data for exactly 1 ambient light to exist");
		
		gr::RenderDataID ambientID = renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataAmbientIBL>()[0];

		if (renderData.IsDirty<gr::Light::RenderDataAmbientIBL>(ambientID))
		{
			gr::Light::RenderDataAmbientIBL const& ambientRenderData = 
				renderData.GetObjectData<gr::Light::RenderDataAmbientIBL>(ambientID);



			const AmbientLightParams ambientLightParams = GetAmbientLightParamsData(
				totalPMREMMipLevels,
				static_cast<float>(ambientRenderData.m_diffuseEnabled),
				static_cast<float>(ambientRenderData.m_specularEnabled),
				m_AOGS ? m_AOGS->GetFinalTextureTargetSet()->GetColorTarget(0).GetTexture().get() : nullptr);

			m_ambientParams->Commit(ambientLightParams);
		}		
	}


	void DeferredLightingGraphicsSystem::CreateBatches()
	{
		// TODO: We should cache our batches, and only update them (or their attached ParameterBlocks) when their render
		// data changes

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Ambient stage batches:
		SEAssert(renderData.GetNumElementsOfType<gr::Light::RenderDataAmbientIBL>() == 1,
			"We currently expect render data for exactly 1 ambient light to exist");

		std::vector<gr::RenderDataID> const& ambientIDs = 
			renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataAmbientIBL>();

		auto ambientItr = renderData.IDBegin(ambientIDs);

		gr::Light::RenderDataAmbientIBL const& ambientRenderData = ambientItr.Get<gr::Light::RenderDataAmbientIBL>();
		gr::MeshPrimitive::RenderData const& ambientMeshPrimData = ambientItr.Get<gr::MeshPrimitive::RenderData>();

		const re::Batch ambientFullscreenQuadBatch = 
			re::Batch(re::Batch::Lifetime::SingleFrame, ambientMeshPrimData, nullptr);

		m_ambientStage->AddBatch(ambientFullscreenQuadBatch);

		// Directional stage batches:
		if (renderData.HasObjectData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> const& directionalIDs =
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataDirectional>();

			SEAssert(directionalIDs.size() == 1,
				"We currently assume there will only be 1 directional light (even though it's not necessary to)");

			auto directionalItr = renderData.IDBegin(directionalIDs);
			auto const& directionalItrEnd = renderData.IDEnd(directionalIDs);
			while (directionalItr != directionalItrEnd)
			{
				gr::Light::RenderDataDirectional const& directionalData = 
					directionalItr.Get<gr::Light::RenderDataDirectional>();

				if (directionalData.m_colorIntensity.w > 0.f && 
					(directionalData.m_diffuseEnabled ||
						directionalData.m_specularEnabled))
				{
					gr::Transform::RenderData const& directionalTransformData = directionalItr.GetTransformData();

					gr::ShadowMap::RenderData const* shadowData = nullptr;
					gr::Camera::RenderData const* shadowCamData = nullptr;
					if (directionalData.m_hasShadow)
					{
						shadowData = &renderData.GetObjectData<gr::ShadowMap::RenderData>(directionalData.m_renderDataID);
						shadowCamData = &renderData.GetObjectData<gr::Camera::RenderData>(directionalData.m_renderDataID);
					}

					LightParams const& directionalParams = GetLightParamData(
						&directionalData,
						gr::Light::Type::Directional,
						directionalTransformData,
						shadowData,
						shadowCamData,
						m_directionalStage->GetTextureTargetSet());

					std::shared_ptr<re::ParameterBlock> directionalPB = re::ParameterBlock::Create(
						LightParams::s_shaderName,
						directionalParams,
						re::ParameterBlock::PBType::SingleFrame);

					gr::MeshPrimitive::RenderData const& meshData = directionalItr.Get<gr::MeshPrimitive::RenderData>();

					re::Batch directionalBatch = re::Batch(re::Batch::Lifetime::SingleFrame, meshData, nullptr);

					directionalBatch.SetParameterBlock(directionalPB);

					m_directionalStage->AddBatch(directionalBatch);
				}

				++directionalItr;
			}
		}


		// Point light stage batches:
		if (renderData.HasObjectData<gr::Light::RenderDataPoint>())
		{
			std::vector<gr::RenderDataID> const& pointIDs =
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataPoint>();

			auto pointItr = renderData.IDBegin(pointIDs);
			auto const& pointItrEnd = renderData.IDEnd(pointIDs);
			while (pointItr != pointItrEnd)
			{
				gr::Light::RenderDataPoint const& pointData = pointItr.Get<gr::Light::RenderDataPoint>();
				if (pointData.m_colorIntensity.w > 0.f && (pointData.m_diffuseEnabled || pointData.m_specularEnabled))
				{
					gr::MeshPrimitive::RenderData const& meshData = pointItr.Get<gr::MeshPrimitive::RenderData>();

					re::Batch pointlightBatch = re::Batch(re::Batch::Lifetime::SingleFrame, meshData, nullptr);

					// Point light params:
					gr::Transform::RenderData const& transformData = pointItr.GetTransformData();

					pointlightBatch.SetParameterBlock(gr::Transform::CreateInstancedTransformParams(
						re::ParameterBlock::PBType::SingleFrame, transformData));

					gr::ShadowMap::RenderData const* shadowData = nullptr;
					gr::Camera::RenderData const* shadowCamData = nullptr;
					const bool hasShadow = pointData.m_hasShadow;
					if (hasShadow)
					{
						shadowData = &pointItr.Get<gr::ShadowMap::RenderData>();
						shadowCamData = &pointItr.Get<gr::Camera::RenderData>();
					}

					LightParams const& pointLightParams = GetLightParamData(
						&pointData,
						gr::Light::Type::Point,
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
						re::Texture const* shadowMap =
							m_shadowGS->GetShadowMap(gr::Light::Point, pointData.m_renderDataID);

						std::shared_ptr<re::Sampler> sampler =
							re::Sampler::GetSampler(re::Sampler::WrapAndFilterMode::Wrap_Linear_Linear);

						pointlightBatch.AddTextureAndSamplerInput("CubeMap0", shadowMap, sampler);
					}

					// Finally, add the completed batch:
					m_pointStage->AddBatch(pointlightBatch);
				}

				++pointItr;
			}
		}
	}
}