// © 2022 Adam Badke. All rights reserved.
#include "BatchManager.h"
#include "Config.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystem_XeGTAO.h"
#include "GraphicsSystemManager.h"
#include "LightRenderData.h"
#include "MeshFactory.h"
#include "ParameterBlock.h"
#include "Sampler.h"
#include "Shader.h"
#include "ShadowMapRenderData.h"
#include "RenderDataManager.h"
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
		glm::vec4 g_intensityScaleShadowed; // .xy = diffuse/specular intensity scale, .z = has shadow (1.f), w = unused

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
		bool shadowEnabled = false;
		bool diffuseEnabled = false;
		bool specEnabled = false;
		switch (lightType)
		{
		case fr::Light::Type::Directional:
		{
			gr::Light::RenderDataDirectional const* directionalData = 
				static_cast<gr::Light::RenderDataDirectional const*>(lightRenderData);

			SEAssert((directionalData->m_hasShadow == (shadowData != nullptr)) && 
				(directionalData->m_hasShadow == (shadowCamData != nullptr)),
				"A shadow requires both shadow and camera data");
			
			lightParams.g_lightColorIntensity = directionalData->m_colorIntensity;

			// As per the KHR_lights_punctual, directional lights are at infinity and emit light in the direction of the
			// local -z axis. Thus, this direction is pointing towards the source of the light
			// https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#directional
			lightParams.g_lightWorldPosRadius = glm::vec4(transformData.m_globalForward, 0.f); // WorldPos == Dir to light

			hasShadow = directionalData->m_hasShadow;
			shadowEnabled = hasShadow && shadowData->m_shadowEnabled;
			diffuseEnabled = directionalData->m_diffuseEnabled;
			specEnabled = directionalData->m_specularEnabled;
		}
		break;
		case fr::Light::Type::Point:
		{
			gr::Light::RenderDataPoint const* pointData =
				static_cast<gr::Light::RenderDataPoint const*>(lightRenderData);
			
			SEAssert((pointData->m_hasShadow == (shadowData != nullptr)) &&
				(pointData->m_hasShadow == (shadowCamData != nullptr)),
				"A shadow requires both shadow and camera data");

			lightParams.g_lightColorIntensity = pointData->m_colorIntensity;
			
			lightParams.g_lightWorldPosRadius = 
				glm::vec4(transformData.m_globalPosition, pointData->m_emitterRadius);

			hasShadow = pointData->m_hasShadow;
			shadowEnabled = hasShadow && shadowData->m_shadowEnabled;
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
		
		lightParams.g_intensityScaleShadowed = glm::vec4(
			static_cast<float>(diffuseEnabled),
			static_cast<float>(specEnabled),
			static_cast<float>(shadowEnabled),
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
		, m_AOGS(nullptr)
		, m_resourceCreationStagePipeline(nullptr)
	{
	}


	void DeferredLightingGraphicsSystem::CreateSingleFrameBRDFPreIntegrationStage(
		re::StagePipeline& pipeline)
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


	void DeferredLightingGraphicsSystem::PopulateIEMTex(
		re::StagePipeline* pipeline, re::Texture const* iblTex, std::shared_ptr<re::Texture>& iemTexOut) const
	{
		// Common IBL texture generation stage params:
		re::PipelineState iblStageParams;
		iblStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
		iblStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);

		std::shared_ptr<re::Shader> iemShader =
			re::Shader::GetOrCreate(en::ShaderNames::k_generateIEMShaderName, iblStageParams);

		const uint32_t iemTexWidthHeight =
			static_cast<uint32_t>(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_iemTexWidthHeight));

		// IEM-specific texture params:
		re::Texture::TextureParams iemTexParams;
		iemTexParams.m_width = iemTexWidthHeight;
		iemTexParams.m_height = iemTexWidthHeight;
		iemTexParams.m_faces = 6;
		iemTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::Color);
		iemTexParams.m_dimension = re::Texture::Dimension::TextureCubeMap;
		iemTexParams.m_format = re::Texture::Format::RGBA16F;
		iemTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		iemTexParams.m_addToSceneData = false;
		iemTexParams.m_mipMode = re::Texture::MipMode::None;

		const std::string IEMTextureName = iblTex->GetName() + "_IEMTexture";
		iemTexOut = re::Texture::Create(IEMTextureName, iemTexParams);

		for (uint32_t face = 0; face < 6; face++)
		{
			re::RenderStage::GraphicsStageParams gfxStageParams;
			std::shared_ptr<re::RenderStage> iemStage = re::RenderStage::CreateSingleFrameGraphicsStage(
				"IEM generation: Face " + std::to_string(face + 1) + "/6", gfxStageParams);

			iemStage->SetStageShader(iemShader);
			iemStage->AddTextureInput(
				"Tex0",
				iblTex,
				re::Sampler::GetSampler("ClampMinMagMipLinear"));

			// Parameter blocks:
			IEMPMREMGenerationParams const& iemGenerationParams =
				GetIEMPMREMGenerationParamsData(0, 1, face, iblTex->Width(), iblTex->Height());
			std::shared_ptr<re::ParameterBlock> iemGenerationPB = re::ParameterBlock::Create(
				IEMPMREMGenerationParams::s_shaderName,
				iemGenerationParams,
				re::ParameterBlock::PBType::SingleFrame);
			iemStage->AddSingleFrameParameterBlock(iemGenerationPB);

			iemStage->AddPermanentParameterBlock(m_cubemapRenderCamParams[face]);

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

			iemTargets->SetColorTarget(0, iemTexOut, targetParams);
			iemTargets->SetViewport(re::Viewport(0, 0, iemTexWidthHeight, iemTexWidthHeight));
			iemTargets->SetScissorRect(re::ScissorRect(0, 0, iemTexWidthHeight, iemTexWidthHeight));

			iemStage->SetTextureTargetSet(iemTargets);

			iemStage->AddBatch(*m_cubeMeshBatch);

			pipeline->AppendSingleFrameRenderStage(std::move(iemStage));
		}
	}


	void DeferredLightingGraphicsSystem::PopulatePMREMTex(
		re::StagePipeline* pipeline, re::Texture const* iblTex, std::shared_ptr<re::Texture>& pmremTexOut) const
	{
		// Common IBL texture generation stage params:
		re::PipelineState iblStageParams;
		iblStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Disabled);
		iblStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::Always);

		std::shared_ptr<re::Shader> pmremShader =
			re::Shader::GetOrCreate(en::ShaderNames::k_generatePMREMShaderName, iblStageParams);

		const uint32_t pmremTexWidthHeight =
			static_cast<uint32_t>(en::Config::Get()->GetValue<int>(en::ConfigKeys::k_pmremTexWidthHeight));

		// PMREM-specific texture params:
		re::Texture::TextureParams pmremTexParams;
		pmremTexParams.m_width = pmremTexWidthHeight;
		pmremTexParams.m_height = pmremTexWidthHeight;
		pmremTexParams.m_faces = 6;
		pmremTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::Color);
		pmremTexParams.m_dimension = re::Texture::Dimension::TextureCubeMap;
		pmremTexParams.m_format = re::Texture::Format::RGBA16F;
		pmremTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		pmremTexParams.m_addToSceneData = false;
		pmremTexParams.m_mipMode = re::Texture::MipMode::Allocate;

		const std::string PMREMTextureName = iblTex->GetName() + "_PMREMTexture";
		pmremTexOut = re::Texture::Create(PMREMTextureName, pmremTexParams);

		const uint32_t totalMipLevels = pmremTexOut->GetNumMips();

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
					iblTex,
					re::Sampler::GetSampler("ClampMinMagMipLinear"));

				// Parameter blocks:
				IEMPMREMGenerationParams const& pmremGenerationParams = GetIEMPMREMGenerationParamsData(
					currentMipLevel, totalMipLevels, face, iblTex->Width(), iblTex->Height());
				std::shared_ptr<re::ParameterBlock> pmremGenerationPB = re::ParameterBlock::Create(
					IEMPMREMGenerationParams::s_shaderName,
					pmremGenerationParams,
					re::ParameterBlock::PBType::SingleFrame);
				pmremStage->AddSingleFrameParameterBlock(pmremGenerationPB);

				pmremStage->AddPermanentParameterBlock(m_cubemapRenderCamParams[face]);

				re::TextureTarget::TargetParams targetParams;
				targetParams.m_targetFace = face;
				targetParams.m_targetMip = currentMipLevel;

				std::shared_ptr<re::TextureTargetSet> pmremTargetSet =
					re::TextureTargetSet::Create("PMREM texture targets: Face " + postFix);

				pmremTargetSet->SetColorTarget(0, pmremTexOut, targetParams);

				const glm::vec4 mipDimensions =
					pmremTexOut->GetSubresourceDimensions(currentMipLevel);
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

				pmremStage->AddBatch(*m_cubeMeshBatch);

				pipeline->AppendSingleFrameRenderStage(std::move(pmremStage));
			}
		}
	}


	void DeferredLightingGraphicsSystem::CreateResourceGenerationStages(re::StagePipeline& pipeline)
	{
		m_resourceCreationStagePipeline = &pipeline;

		m_resourceCreationStageParentItr = pipeline.AppendRenderStage(
			re::RenderStage::CreateParentStage("Resource creation stages parent"));


		// Cube mesh, for rendering of IBL cubemaps
		if (m_cubeMeshPrimitive == nullptr)
		{
			m_cubeMeshPrimitive = gr::meshfactory::CreateCube();
		}

		// Create a cube mesh batch, for reuse during the initial frame IBL rendering:
		if (m_cubeMeshBatch == nullptr)
		{
			m_cubeMeshBatch = std::make_unique<re::Batch>(re::Batch::Lifetime::Permanent, m_cubeMeshPrimitive.get());
		}

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

		for (uint8_t face = 0; face < 6; face++)
		{
			if (m_cubemapRenderCamParams[face] == nullptr)
			{
				cubemapCamParams.g_view = cubemapViews[face];

				m_cubemapRenderCamParams[face] = re::ParameterBlock::Create(
					gr::Camera::CameraParams::s_shaderName,
					cubemapCamParams,
					re::ParameterBlock::PBType::Immutable);
			}
		}

		// 1st frame: Generate the pre-integrated BRDF LUT via a single-frame compute stage:
		CreateSingleFrameBRDFPreIntegrationStage(pipeline);
	}


	void DeferredLightingGraphicsSystem::Create(re::RenderSystem& renderSystem, re::StagePipeline& pipeline)
	{
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_ambientStage = re::RenderStage::CreateGraphicsStage("Ambient light stage", gfxStageParams);

		m_directionalStage = re::RenderStage::CreateGraphicsStage("Directional light stage", gfxStageParams);
		m_pointStage = re::RenderStage::CreateGraphicsStage("Point light stage", gfxStageParams);

		m_shadowGS = m_graphicsSystemManager->GetGraphicsSystem<gr::ShadowsGraphicsSystem>();
		SEAssert(m_shadowGS != nullptr, "Shadow graphics system not found");

		GBufferGraphicsSystem* gBufferGS = 
			m_graphicsSystemManager->GetGraphicsSystem<GBufferGraphicsSystem>();
		SEAssert(gBufferGS != nullptr, "GBuffer GS not found");
		

		// Create a lighting texture target:
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

		std::shared_ptr<re::Texture> lightTargetTex = re::Texture::Create("DeferredLightTarget", lightTargetTexParams);

		// Create the lighting target set (shared by all lights/stages):
		re::TextureTarget::TargetParams deferredTargetParams{};
		deferredTargetParams.m_clearMode = re::TextureTarget::TargetParams::ClearMode::Disabled;

		// We need the depth buffer attached, but with depth writes disabled:
		re::TextureTarget::TargetParams depthTargetParams;
		depthTargetParams.m_channelWriteMode.R = re::TextureTarget::TargetParams::ChannelWrite::Disabled;

		std::shared_ptr<re::TextureTargetSet> deferredTargetSet = re::TextureTargetSet::Create("Deferred light targets");
		deferredTargetSet->SetColorTarget(0, lightTargetTex, deferredTargetParams);

		deferredTargetSet->SetDepthStencilTarget(
			gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
			depthTargetParams);

		// All deferred lighting is additive
		re::TextureTarget::TargetParams::BlendModes deferredBlendModes
		{
			re::TextureTarget::TargetParams::BlendMode::One,
			re::TextureTarget::TargetParams::BlendMode::One,
		};
		deferredTargetSet->SetColorTargetBlendModes(1, &deferredBlendModes);


		// Append a color-only clear stage to clear the lighting target:
		re::RenderStage::ClearStageParams colorClearParams;
		colorClearParams.m_colorClearModes = { re::TextureTarget::TargetParams::ClearMode::Enabled };
		colorClearParams.m_depthClearMode = re::TextureTarget::TargetParams::ClearMode::Disabled;
		pipeline.AppendRenderStage(re::RenderStage::CreateClearStage(colorClearParams, deferredTargetSet));


		// Ambient stage:
		// --------------
		m_ambientStage->SetTextureTargetSet(deferredTargetSet);

		re::PipelineState fullscreenQuadStageParams;

		// Ambient/directional lights use back face culling, as they're fullscreen quads.
		// Our fullscreen quad is on the far plane; We only want to light something if the quad is behind the geo (i.e.
		// the quad's depth is greater than what is in the depth buffer)
		fullscreenQuadStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back); 
		fullscreenQuadStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::Greater);
		
		// Ambient light stage:
		m_ambientStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_deferredAmbientLightShaderName, fullscreenQuadStageParams));

		m_ambientStage->AddPermanentParameterBlock(m_graphicsSystemManager->GetActiveCameraParams());	
		
		// Get/set the AO texture, if it exists:
		m_AOGS = m_graphicsSystemManager->GetGraphicsSystem<XeGTAOGraphicsSystem>();
		re::Texture const* ssaoTex = nullptr;
		if (m_AOGS)
		{
			m_ambientStage->AddTextureInput(
				"SSAOTex",
				m_AOGS->GetFinalTextureTargetSet()->GetColorTarget(0).GetTexture(),
				re::Sampler::GetSampler("ClampMinMagMipPoint"));

			ssaoTex = m_AOGS->GetFinalTextureTargetSet()->GetColorTarget(0).GetTexture().get();
		}

		// Append the ambient stage:
		pipeline.AppendRenderStage(m_ambientStage);
		

		// Directional light stage:
		//-------------------------
		m_directionalStage->SetTextureTargetSet(deferredTargetSet);

		m_directionalStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_deferredDirectionalLightShaderName, fullscreenQuadStageParams));

		m_directionalStage->AddPermanentParameterBlock(m_graphicsSystemManager->GetActiveCameraParams());

		pipeline.AppendRenderStage(m_directionalStage);


		// Point light stage:
		//-------------------
		m_pointStage->SetTextureTargetSet(deferredTargetSet);

		m_pointStage->AddPermanentParameterBlock(m_graphicsSystemManager->GetActiveCameraParams());

		// Point lights only illuminate something if the sphere volume is behind it
		re::PipelineState pointStageParams(fullscreenQuadStageParams);
		pointStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::GEqual);
		pointStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Front); // Cull front faces of light volumes

		m_pointStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_deferredPointLightShaderName, pointStageParams));

		pipeline.AppendRenderStage(m_pointStage);


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

			m_ambientStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[slot],
				gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
				re::Sampler::GetSampler("WrapMinMagLinearMipPoint"));

			m_directionalStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[slot],
				gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
				re::Sampler::GetSampler("WrapMinMagLinearMipPoint"));

			m_pointStage->AddTextureInput(
				GBufferGraphicsSystem::GBufferTexNames[slot],
				gBufferGS->GetFinalTextureTargetSet()->GetColorTarget(slot).GetTexture(),
				re::Sampler::GetSampler("WrapMinMagLinearMipPoint"));
		}


		// Attach the GBUffer depth input:
		constexpr uint8_t depthBufferSlot = gr::GBufferGraphicsSystem::GBufferDepth;

		m_directionalStage->AddTextureInput(
			GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
			gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
			re::Sampler::GetSampler("WrapMinMagLinearMipPoint"));

		m_pointStage->AddTextureInput(
			GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
			gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
			re::Sampler::GetSampler("WrapMinMagLinearMipPoint"));

		m_ambientStage->AddTextureInput(
			GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot],
			gBufferGS->GetFinalTextureTargetSet()->GetDepthStencilTarget()->GetTexture(),
			re::Sampler::GetSampler("WrapMinMagLinearMipPoint"));

		m_ambientStage->AddTextureInput(
			"Tex7",
			m_BRDF_integrationMap,
			re::Sampler::GetSampler("ClampMinMagMipPoint"));
	}


	void DeferredLightingGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Removed any deleted directional/point lights:
		auto DeleteLights = []<typename T>(
			std::vector<gr::RenderDataID> const& deletedIDs, std::unordered_map<gr::RenderDataID, T>&stageData)
		{
			for (gr::RenderDataID id : deletedIDs)
			{
				stageData.erase(id);
			}
		};
		if (renderData.HasIDsWithDeletedData<gr::Light::RenderDataAmbientIBL>())
		{
			DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataAmbientIBL>(), m_ambientLightData);
		}
		if (renderData.HasIDsWithDeletedData<gr::Light::RenderDataDirectional>())
		{
			DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataDirectional>(), m_punctualLightData);
		}
		if (renderData.HasIDsWithDeletedData<gr::Light::RenderDataPoint>())
		{
			DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataPoint>(), m_punctualLightData);
		}


		// Register new ambient lights:
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataAmbientIBL>())
		{
			std::vector<gr::RenderDataID> const& newAmbientIDs =
				renderData.GetIDsWithNewData<gr::Light::RenderDataAmbientIBL>();

			auto ambientItr = renderData.IDBegin(newAmbientIDs);
			auto const& ambientItrEnd = renderData.IDEnd(newAmbientIDs);
			while (ambientItr != ambientItrEnd)
			{
				gr::Light::RenderDataAmbientIBL const& ambientData = ambientItr.Get<gr::Light::RenderDataAmbientIBL>();
				
				const gr::RenderDataID lightID = ambientData.m_renderDataID;

				gr::MeshPrimitive::RenderData const& ambientMeshPrimData = 
					ambientItr.Get<gr::MeshPrimitive::RenderData>();

				re::Texture const* iblTex = ambientData.m_iblTex;
				SEAssert(iblTex, "IBL texture cannot be null");

				std::shared_ptr<re::Texture> iemTex = nullptr;
				PopulateIEMTex(m_resourceCreationStagePipeline, iblTex, iemTex);

				std::shared_ptr<re::Texture> pmremTex = nullptr;
				PopulatePMREMTex(m_resourceCreationStagePipeline, iblTex, pmremTex);

				const uint32_t totalPMREMMipLevels = pmremTex->GetNumMips();

				re::Texture const* ssaoTex = nullptr;
				if (m_AOGS)
				{
					ssaoTex = m_AOGS->GetFinalTextureTargetSet()->GetColorTarget(0).GetTexture().get();
				}

				const AmbientLightParams ambientLightParams = GetAmbientLightParamsData(
					totalPMREMMipLevels,
					ambientData.m_diffuseScale,
					ambientData.m_specularScale,
					ssaoTex);

				std::shared_ptr<re::ParameterBlock> ambientParams = re::ParameterBlock::Create(
					AmbientLightParams::s_shaderName,
					ambientLightParams,
					re::ParameterBlock::PBType::Mutable);

				m_ambientLightData.emplace(ambientData.m_renderDataID,
					AmbientLightData{
						.m_ambientParams = ambientParams,
						.m_IEMTex = iemTex,
						.m_PMREMTex = pmremTex,
						.m_batch = re::Batch(re::Batch::Lifetime::Permanent, ambientMeshPrimData, nullptr)
					});

				// Set the batch inputs:
				re::Batch& ambientBatch = m_ambientLightData.at(lightID).m_batch;

				ambientBatch.AddTextureAndSamplerInput(
					"CubeMap0",
					iemTex.get(),
					re::Sampler::GetSampler("WrapMinMagLinearMipPoint"));

				ambientBatch.AddTextureAndSamplerInput(
					"CubeMap1",
					pmremTex.get(),
					re::Sampler::GetSampler("WrapMinMagMipLinear"));

				ambientBatch.SetParameterBlock(ambientParams);

				++ambientItr;
			}
		}


		// Register new directional lights:
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> const& newDirectionalIDs =
				renderData.GetIDsWithNewData<gr::Light::RenderDataDirectional>();

			auto directionalItr = renderData.IDBegin(newDirectionalIDs);
			auto const& directionalItrEnd = renderData.IDEnd(newDirectionalIDs);
			while (directionalItr != directionalItrEnd)
			{
				gr::Light::RenderDataDirectional const& directionalData =
					directionalItr.Get<gr::Light::RenderDataDirectional>();

				gr::Transform::RenderData const& directionalTransformData = directionalItr.GetTransformData();
				gr::MeshPrimitive::RenderData const& meshData = directionalItr.Get<gr::MeshPrimitive::RenderData>();

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

				std::shared_ptr<re::ParameterBlock> directionalLightParams = re::ParameterBlock::Create(
					LightParams::s_shaderName,
					directionalParams,
					re::ParameterBlock::PBType::Mutable);

				m_punctualLightData.emplace(
					directionalItr.GetRenderDataID(),
					PunctualLightData{
						.m_type = gr::Light::Directional,
						.m_lightParams = directionalLightParams,
						.m_transformParams = nullptr,
						.m_batch = re::Batch(re::Batch::Lifetime::Permanent, meshData, nullptr)
					});

				re::Batch& directionalLightBatch = m_punctualLightData.at(directionalData.m_renderDataID).m_batch;

				directionalLightBatch.SetParameterBlock(directionalLightParams);

				if (directionalData.m_hasShadow) // Add the shadow map texture to the batch
				{
					directionalLightBatch.AddTextureAndSamplerInput(
						"Depth0",
						m_shadowGS->GetShadowMap(gr::Light::Type::Directional, directionalData.m_renderDataID),
						re::Sampler::GetSampler("BorderCmpMinMagLinearMipPoint"));
				}

				++directionalItr;
			}
		}

		// Register new point lights:
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataPoint>())
		{
			std::vector<gr::RenderDataID> const& newPointIDs =
				renderData.GetIDsWithNewData<gr::Light::RenderDataPoint>();

			auto pointItr = renderData.IDBegin(newPointIDs);
			auto const& pointItrEnd = renderData.IDEnd(newPointIDs);
			while (pointItr != pointItrEnd)
			{
				gr::Light::RenderDataPoint const& pointData = pointItr.Get<gr::Light::RenderDataPoint>();

				gr::MeshPrimitive::RenderData const& meshData = pointItr.Get<gr::MeshPrimitive::RenderData>();
				gr::Transform::RenderData const& transformData = pointItr.GetTransformData();

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
					re::ParameterBlock::PBType::Mutable);

				std::shared_ptr<re::ParameterBlock> transformPB = gr::Transform::CreateInstancedTransformParams(
					re::ParameterBlock::PBType::Mutable, transformData);

				m_punctualLightData.emplace(
					pointItr.GetRenderDataID(),
					PunctualLightData{
						.m_type = gr::Light::Point,
						.m_lightParams = pointlightPB,
						.m_transformParams = transformPB,
						.m_batch = re::Batch(re::Batch::Lifetime::Permanent, meshData, nullptr)
					});

				re::Batch& pointlightBatch = m_punctualLightData.at(pointData.m_renderDataID).m_batch;

				pointlightBatch.SetParameterBlock(pointlightPB);

				pointlightBatch.SetParameterBlock(transformPB);

				if (hasShadow) // Add the shadow map texture to the batch
				{
					pointlightBatch.AddTextureAndSamplerInput(
						"CubeDepth",
						m_shadowGS->GetShadowMap(gr::Light::Point, pointData.m_renderDataID),
						re::Sampler::GetSampler("WrapCmpMinMagLinearMipPoint"));
				}

				++pointItr;
			}
		}


		CreateBatches();
	}


	void DeferredLightingGraphicsSystem::CreateBatches()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Update the ambient lights we're tracking:
		for (auto const& ambientLight : m_ambientLightData)
		{
			const gr::RenderDataID lightID = ambientLight.first;

			if (renderData.IsDirty<gr::Light::RenderDataAmbientIBL>(lightID))
			{
				gr::Light::RenderDataAmbientIBL const& ambientRenderData =
					renderData.GetObjectData<gr::Light::RenderDataAmbientIBL>(lightID);

				const uint32_t totalPMREMMipLevels = ambientLight.second.m_PMREMTex->GetNumMips();

				const AmbientLightParams ambientLightParams = GetAmbientLightParamsData(
					totalPMREMMipLevels,
					ambientRenderData.m_diffuseScale,
					ambientRenderData.m_specularScale,
					m_AOGS ? m_AOGS->GetFinalTextureTargetSet()->GetColorTarget(0).GetTexture().get() : nullptr);

				ambientLight.second.m_ambientParams->Commit(ambientLightParams);
			}
		}

		if (m_graphicsSystemManager->HasActiveAmbientLight())
		{
			const gr::RenderDataID activeAmbientID = m_graphicsSystemManager->GetActiveAmbientLightID();

			SEAssert(m_ambientLightData.contains(activeAmbientID), "Cannot find active ambient light");

			m_ambientStage->AddBatch(m_ambientLightData.at(activeAmbientID).m_batch);
		}


		// Update all of the punctual lights we're tracking:
		for (auto const& light : m_punctualLightData)
		{
			const gr::RenderDataID lightID = light.first;

			// Update lighting parameter blocks, if anything is dirty:
			const bool transformIsDirty = renderData.TransformIsDirtyFromRenderDataID(lightID);

			const bool lightRenderDataDirty = 
				(light.second.m_type == gr::Light::Type::Directional &&
					renderData.IsDirty<gr::Light::RenderDataDirectional>(lightID)) ||
				(light.second.m_type == gr::Light::Type::Point &&
					renderData.IsDirty<gr::Light::RenderDataPoint>(lightID));

			const bool shadowDataIsDirty = 
				(renderData.HasObjectData<gr::ShadowMap::RenderData>(lightID) && 
					renderData.IsDirty<gr::ShadowMap::RenderData>(lightID) ) ||
				(renderData.HasObjectData<gr::Camera::RenderData>(lightID) &&
					renderData.IsDirty<gr::Camera::RenderData>(lightID));

			if (transformIsDirty || lightRenderDataDirty || shadowDataIsDirty)
			{
				gr::Transform::RenderData const& lightTransformData =
					renderData.GetTransformDataFromRenderDataID(lightID);

				gr::ShadowMap::RenderData const* shadowData = nullptr;
				gr::Camera::RenderData const* shadowCamData = nullptr;

				switch (light.second.m_type)
				{
				case gr::Light::Type::Directional:
				{
					gr::Light::RenderDataDirectional const& directionalData =
						renderData.GetObjectData<gr::Light::RenderDataDirectional>(lightID);

					if (directionalData.m_hasShadow)
					{
						shadowData = &renderData.GetObjectData<gr::ShadowMap::RenderData>(lightID);
						shadowCamData = &renderData.GetObjectData<gr::Camera::RenderData>(lightID);
					}

					LightParams const& directionalParams = GetLightParamData(
						&directionalData,
						gr::Light::Type::Directional,
						lightTransformData,
						shadowData,
						shadowCamData,
						m_directionalStage->GetTextureTargetSet());

					light.second.m_lightParams->Commit(directionalParams);
				}
				break;
				case gr::Light::Type::Point:
				{
					gr::Light::RenderDataPoint const& pointData = 
						renderData.GetObjectData<gr::Light::RenderDataPoint>(lightID);

					if (pointData.m_hasShadow)
					{
						shadowData = &renderData.GetObjectData<gr::ShadowMap::RenderData>(lightID);
						shadowCamData = &renderData.GetObjectData<gr::Camera::RenderData>(lightID);
					}

					LightParams const& pointLightParams = GetLightParamData(
						&pointData,
						gr::Light::Type::Point,
						lightTransformData,
						shadowData,
						shadowCamData,
						m_pointStage->GetTextureTargetSet());

					light.second.m_lightParams->Commit(pointLightParams);

					light.second.m_transformParams->Commit(gr::Transform::CreateInstancedTransformParamsData(
							renderData.GetTransformDataFromRenderDataID(lightID)));
				}
				break;
				default: SEAssertF("Invalid light type");
				}
			}

			// Add punctual batches:
			// TODO: We should cull these, and only add them if the light is active (ie. non-zero intensity etc)
			switch (light.second.m_type)
			{
			case gr::Light::Type::Directional:
			{
				m_directionalStage->AddBatch(light.second.m_batch);
			}
			break;
			case gr::Light::Type::Point:
			{
				m_pointStage->AddBatch(light.second.m_batch);
			}
			break;
			default: SEAssertF("Invalid light type");
			}
		}
	}
}