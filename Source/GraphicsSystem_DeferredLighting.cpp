// © 2022 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "Core\Config.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystemManager.h"
#include "LightRenderData.h"
#include "MeshFactory.h"
#include "Sampler.h"
#include "Shader.h"
#include "ShadowMapRenderData.h"
#include "RenderDataManager.h"
#include "RenderStage.h"

#include "Shaders\Common\IBLGenerationParams.h"
#include "Shaders\Common\LightParams.h"


namespace
{
	BRDFIntegrationData GetBRDFIntegrationParamsDataData()
	{
		const uint32_t brdfTexWidthHeight =
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey));

		BRDFIntegrationData brdfIntegrationParams{
			.g_integrationTargetResolution =
				glm::uvec4(brdfTexWidthHeight, brdfTexWidthHeight, 0, 0)
		};

		return brdfIntegrationParams;
	}


	IEMPMREMGenerationData GetIEMPMREMGenerationParamsDataData(
		int currentMipLevel, int numMipLevels, uint32_t faceIdx, uint32_t srcWidth, uint32_t srcHeight)
	{
		IEMPMREMGenerationData generationParams;

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

		const int numIEMSamples = core::Config::Get()->GetValue<int>(core::configkeys::k_iemNumSamplesKey);
		const int numPMREMSamples = core::Config::Get()->GetValue<int>(core::configkeys::k_pmremNumSamplesKey);

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


	AmbientLightData GetAmbientLightParamsData(
		uint32_t numPMREMMips, float diffuseScale, float specularScale, re::Texture const* ssaoTex)
	{
		AmbientLightData ambientLightParamsData{};

		const uint32_t dfgTexWidthHeight = 
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey));

		const uint32_t maxPMREMMipLevel = numPMREMMips - 1;

		ambientLightParamsData.g_maxPMREMMipDFGResScaleDiffuseScaleSpec = glm::vec4(
			maxPMREMMipLevel,
			dfgTexWidthHeight,
			diffuseScale,
			specularScale);

		ambientLightParamsData.g_ssaoTexDims = glm::vec4(0.f);
		if (ssaoTex)
		{
			ambientLightParamsData.g_ssaoTexDims = ssaoTex->GetTextureDimenions();
		}

		return ambientLightParamsData;
	}


	LightData GetLightParamData(
		void const* lightRenderData,
		gr::Light::Type lightType,
		gr::Transform::RenderData const& transformData,
		gr::ShadowMap::RenderData const* shadowData,
		gr::Camera::RenderData const* shadowCamData,
		re::TextureTargetSet const* targetSet)
	{
		SEAssert(lightType != gr::Light::Type::AmbientIBL,
			"Ambient lights do not use the LightData structure");

		SEAssert((shadowData != nullptr) == (shadowCamData != nullptr),
			"Shadow data and shadow camera data depend on each other");

		LightData lightParams;
		memset(&lightParams, 0, sizeof(LightData)); // Ensure unused elements are zeroed

		// Direction the light is emitting from the light source. SE uses a RHCS, so this is the local -Z direction
		lightParams.g_globalForwardDir = glm::vec4(transformData.m_globalForward * -1.f, 0.f);

		// Set type-specific params:
		bool hasShadow = false;
		bool shadowEnabled = false;
		bool diffuseEnabled = false;
		bool specEnabled = false;
		glm::vec4 intensityScale(0.f); // Packed below as we go
		glm::vec4 extraParams(0.f);
		switch (lightType)
		{
		case gr::Light::Type::Directional:
		{
			gr::Light::RenderDataDirectional const* directionalData = 
				static_cast<gr::Light::RenderDataDirectional const*>(lightRenderData);

			SEAssert((directionalData->m_hasShadow == (shadowData != nullptr)) && 
				(directionalData->m_hasShadow == (shadowCamData != nullptr)),
				"A shadow requires both shadow and camera data");
			
			lightParams.g_lightColorIntensity = directionalData->m_colorIntensity;

			// As per the KHR_lights_punctual, directional lights are at infinity and emit light in the direction of the
			// local -Z axis. Thus, this direction is pointing towards the source of the light (saves a * -1 on the GPU)
			// https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#directional
			lightParams.g_lightWorldPosRadius = glm::vec4(transformData.m_globalForward, 0.f); // WorldPos == Dir to light

			hasShadow = directionalData->m_hasShadow;
			shadowEnabled = hasShadow && shadowData->m_shadowEnabled;
			diffuseEnabled = directionalData->m_diffuseEnabled;
			specEnabled = directionalData->m_specularEnabled;
		}
		break;
		case gr::Light::Type::Point:
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
		case gr::Light::Type::Spot:
		{
			gr::Light::RenderDataSpot const* spotData = static_cast<gr::Light::RenderDataSpot const*>(lightRenderData);

			SEAssert((spotData->m_hasShadow == (shadowData != nullptr)) &&
				(spotData->m_hasShadow == (shadowCamData != nullptr)),
				"A shadow requires both shadow and camera data");

			lightParams.g_lightColorIntensity = spotData->m_colorIntensity;

			lightParams.g_lightWorldPosRadius = glm::vec4(transformData.m_globalPosition, spotData->m_emitterRadius);

			hasShadow = spotData->m_hasShadow;
			shadowEnabled = hasShadow && shadowData->m_shadowEnabled;
			diffuseEnabled = spotData->m_diffuseEnabled;
			specEnabled = spotData->m_specularEnabled;

			intensityScale.z = spotData->m_innerConeAngle;
			intensityScale.w = spotData->m_outerConeAngle;

			// Extra params:
			const float cosInnerAngle = glm::cos(spotData->m_innerConeAngle);
			const float cosOuterAngle = glm::cos(spotData->m_outerConeAngle);
			
			constexpr float k_divideByZeroEpsilon = 1.0e-5f;
			const float scaleTerm = 1.f / std::max(cosInnerAngle - cosOuterAngle, k_divideByZeroEpsilon);

			const float offsetTerm = -cosOuterAngle * scaleTerm;

			extraParams.x = cosOuterAngle;
			extraParams.y = scaleTerm;
			extraParams.z = offsetTerm;
		}
		break;
		default:
			SEAssertF("Light type does not use this buffer");
		}

		intensityScale.x = diffuseEnabled;
		intensityScale.y = specEnabled;
		
		lightParams.g_intensityScale = intensityScale;

		// Shadow params:
		if (hasShadow)
		{
			switch (lightType)
			{
			case gr::Light::Type::Directional:
			{
				lightParams.g_shadowCam_VP = shadowCamData->m_cameraParams.g_viewProjection;
			}
			break;
			case gr::Light::Type::Point:
			{
				lightParams.g_shadowCam_VP = glm::mat4(0.0f); // Unused by point light cube map shadows
			}
			break;
			case gr::Light::Type::Spot:
			{
				lightParams.g_shadowCam_VP = shadowCamData->m_cameraParams.g_viewProjection;
			}
			break;
			default:
				SEAssertF("Light shadow type does not use this buffer");
			}

			lightParams.g_shadowMapTexelSize = shadowData->m_textureDims;

			lightParams.g_shadowCamNearFarBiasMinMax = glm::vec4(
				shadowCamData->m_cameraConfig.m_near,
				shadowCamData->m_cameraConfig.m_far,
				shadowData->m_minMaxShadowBias);

			lightParams.g_shadowParams = glm::vec4(
				static_cast<float>(shadowEnabled),
				static_cast<float>(shadowData->m_shadowQuality),
				shadowData->m_softness, // [0,1] uv radius X
				shadowData->m_softness); // [0,1] uv radius Y
		}
		else
		{
			lightParams.g_shadowCam_VP = glm::mat4(0.0f);
			lightParams.g_shadowMapTexelSize = glm::vec4(0.f);
			lightParams.g_shadowCamNearFarBiasMinMax = glm::vec4(0.f);
			lightParams.g_shadowParams = glm::vec4(0.f);
		}

		lightParams.g_renderTargetResolution = targetSet->GetTargetDimensions();

		lightParams.g_extraParams = extraParams;

		return lightParams;
	}


	PoissonSampleParamsData GetPoissonSampleParamsData()
	{
		PoissonSampleParamsData shadowSampleParams{};

		memcpy(shadowSampleParams.g_poissonSamples64,
			PoissonSampleParamsData::k_poissonSamples64.data(),
			PoissonSampleParamsData::k_poissonSamples64.size() * sizeof(glm::vec2));
		
		memcpy(shadowSampleParams.g_poissonSamples32,
			PoissonSampleParamsData::k_poissonSamples32.data(),
			PoissonSampleParamsData::k_poissonSamples32.size() * sizeof(glm::vec2));

		memcpy(shadowSampleParams.g_poissonSamples25,
			PoissonSampleParamsData::k_poissonSamples25.data(),
			PoissonSampleParamsData::k_poissonSamples25.size() * sizeof(glm::vec2));

		return shadowSampleParams;
	}
}


namespace gr
{
	constexpr char const* k_gsName = "Deferred Lighting Graphics System";


	DeferredLightingGraphicsSystem::DeferredLightingGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, INamedObject(k_gsName)
		, m_resourceCreationStagePipeline(nullptr)
	{
		m_lightingTargetSet = re::TextureTargetSet::Create("Deferred light targets");
	}


	void DeferredLightingGraphicsSystem::RegisterInputs()
	{
		// Deferred lighting GS is (currently) tightly coupled to the GBuffer GS
		constexpr uint8_t numGBufferColorInputs =
			static_cast<uint8_t>(GBufferGraphicsSystem::GBufferTexIdx::GBufferColorTex_Count);
		for (uint8_t slot = 0; slot < numGBufferColorInputs; slot++)
		{
			if (GBufferGraphicsSystem::GBufferTexNames[slot] == "GBufferEmissive")
			{
				continue;
			}

			RegisterTextureInput(GBufferGraphicsSystem::GBufferTexNames[slot]);
		}
		RegisterTextureInput(GBufferGraphicsSystem::GBufferTexNames[GBufferGraphicsSystem::GBufferTexIdx::GBufferDepth]);

		RegisterTextureInput(k_ssaoInput, TextureInputDefault::OpaqueWhite);

		RegisterDataInput(k_pointLightCullingInput);
		RegisterDataInput(k_spotLightCullingInput);
		RegisterDataInput(k_shadowTexturesInput);
	}


	void DeferredLightingGraphicsSystem::RegisterOutputs()
	{
		RegisterTextureOutput(k_lightOutput, m_lightingTargetSet->GetColorTarget(0).GetTexture());
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
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey));

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

		BRDFIntegrationData const& brdfIntegrationParams = GetBRDFIntegrationParamsDataData();
		std::shared_ptr<re::Buffer> brdfIntegrationBuf = re::Buffer::Create(
			BRDFIntegrationData::s_shaderName,
			brdfIntegrationParams,
			re::Buffer::Type::SingleFrame);
		brdfStage->AddSingleFrameBuffer(brdfIntegrationBuf);

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
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_iemTexWidthHeightKey));

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
				std::format("IEM generation: Face {}/6", face + 1).c_str(), gfxStageParams);

			iemStage->SetStageShader(iemShader);
			iemStage->AddTextureInput(
				"Tex0",
				iblTex,
				re::Sampler::GetSampler("ClampMinMagMipLinear"));

			// Buffers:
			IEMPMREMGenerationData const& iemGenerationParams =
				GetIEMPMREMGenerationParamsDataData(0, 1, face, iblTex->Width(), iblTex->Height());
			std::shared_ptr<re::Buffer> iemGenerationBuffer = re::Buffer::Create(
				IEMPMREMGenerationData::s_shaderName,
				iemGenerationParams,
				re::Buffer::Type::SingleFrame);
			iemStage->AddSingleFrameBuffer(iemGenerationBuffer);

			iemStage->AddPermanentBuffer(m_cubemapRenderCamParams[face]);

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
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_pmremTexWidthHeightKey));

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
					stageName.c_str(), gfxStageParams);

				pmremStage->SetStageShader(pmremShader);
				pmremStage->AddTextureInput(
					"Tex0",
					iblTex,
					re::Sampler::GetSampler("ClampMinMagMipLinear"));

				// Buffers:
				IEMPMREMGenerationData const& pmremGenerationParams = GetIEMPMREMGenerationParamsDataData(
					currentMipLevel, totalMipLevels, face, iblTex->Width(), iblTex->Height());
				std::shared_ptr<re::Buffer> pmremGenerationBuffer = re::Buffer::Create(
					IEMPMREMGenerationData::s_shaderName,
					pmremGenerationParams,
					re::Buffer::Type::SingleFrame);
				pmremStage->AddSingleFrameBuffer(pmremGenerationBuffer);

				pmremStage->AddPermanentBuffer(m_cubemapRenderCamParams[face]);

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


	void DeferredLightingGraphicsSystem::InitializeResourceGenerationStages(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies)
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
		CameraData cubemapCamParams{};

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

				m_cubemapRenderCamParams[face] = re::Buffer::Create(
					CameraData::s_shaderName,
					cubemapCamParams,
					re::Buffer::Type::Immutable);
			}
		}

		// 1st frame: Generate the pre-integrated BRDF LUT via a single-frame compute stage:
		CreateSingleFrameBRDFPreIntegrationStage(pipeline);
	}


	void DeferredLightingGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies)
	{
		m_missing2DShadowFallback = re::Texture::Create("Missing 2D shadow fallback",
			re::Texture::TextureParams
			{
				.m_usage = re::Texture::Usage::Color,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = re::Texture::Format::Depth32F,
				.m_colorSpace = re::Texture::ColorSpace::Linear,
				.m_mipMode = re::Texture::MipMode::None,
				.m_addToSceneData = false,
				.m_clear = re::Texture::TextureParams::ClearValues{},
			},
			glm::vec4(1.f, 1.f, 1.f, 1.f));

		m_missingCubeShadowFallback = re::Texture::Create("Missing cubemap shadow fallback", 
			re::Texture::TextureParams
			{
				.m_faces = 6,
				.m_usage = re::Texture::Usage::Color,
				.m_dimension = re::Texture::Dimension::TextureCubeMap,
				.m_format = re::Texture::Format::Depth32F,
				.m_colorSpace = re::Texture::ColorSpace::Linear,
				.m_mipMode = re::Texture::MipMode::None,
				.m_addToSceneData = false,
				.m_clear = re::Texture::TextureParams::ClearValues{},
			},
			glm::vec4(1.f, 1.f, 1.f, 1.f));

		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_ambientStage = re::RenderStage::CreateGraphicsStage("Ambient light stage", gfxStageParams);

		m_directionalStage = re::RenderStage::CreateGraphicsStage("Directional light stage", gfxStageParams);
		m_pointStage = re::RenderStage::CreateGraphicsStage("Point light stage", gfxStageParams);
		m_spotStage = re::RenderStage::CreateGraphicsStage("Spot light stage", gfxStageParams);

		// Create a lighting texture target:
		re::Texture::TextureParams lightTargetTexParams;
		lightTargetTexParams.m_width = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		lightTargetTexParams.m_height = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);
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

		m_lightingTargetSet->SetColorTarget(0, lightTargetTex, deferredTargetParams);

		m_lightingTargetSet->SetDepthStencilTarget(
			texDependencies.at(GBufferGraphicsSystem::GBufferTexNames[GBufferGraphicsSystem::GBufferDepth]),
			depthTargetParams);

		// All deferred lighting is additive
		re::TextureTarget::TargetParams::BlendModes deferredBlendModes
		{
			re::TextureTarget::TargetParams::BlendMode::One,
			re::TextureTarget::TargetParams::BlendMode::One,
		};
		m_lightingTargetSet->SetColorTargetBlendModes(1, &deferredBlendModes);


		// Append a color-only clear stage to clear the lighting target:
		re::RenderStage::ClearStageParams colorClearParams;
		colorClearParams.m_colorClearModes = { re::TextureTarget::TargetParams::ClearMode::Enabled };
		colorClearParams.m_depthClearMode = re::TextureTarget::TargetParams::ClearMode::Disabled;
		pipeline.AppendRenderStage(re::RenderStage::CreateClearStage(colorClearParams, m_lightingTargetSet));


		// Ambient stage:
		// --------------
		m_ambientStage->SetTextureTargetSet(m_lightingTargetSet);

		re::PipelineState fullscreenQuadStageParams;

		// Ambient/directional lights use back face culling, as they're fullscreen quads.
		// Our fullscreen quad is on the far plane; We only want to light something if the quad is behind the geo (i.e.
		// the quad's depth is greater than what is in the depth buffer)
		fullscreenQuadStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back); 
		fullscreenQuadStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::Greater);
		
		// Ambient light stage:
		m_ambientStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_deferredAmbientLightShaderName, fullscreenQuadStageParams));

		m_ambientStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());	

		// Get/set the AO texture. If it doesn't exist, we'll get a default opaque white texture
		m_ssaoTex = texDependencies.at(k_ssaoInput);

		std::shared_ptr<re::Sampler> clampMinMagMipPoint = re::Sampler::GetSampler("ClampMinMagMipPoint");

		m_ambientStage->AddTextureInput(k_ssaoInput, m_ssaoTex, clampMinMagMipPoint);


		// Append the ambient stage:
		pipeline.AppendRenderStage(m_ambientStage);
		

		// Common PCSS sampling params:
		PoissonSampleParamsData const& poissonSampleParamsData = GetPoissonSampleParamsData();

		std::shared_ptr<re::Buffer> poissonSampleParams = re::Buffer::Create(
			PoissonSampleParamsData::s_shaderName,
			poissonSampleParamsData,
			re::Buffer::Type::Immutable);


		// Directional light stage:
		//-------------------------
		m_directionalStage->SetTextureTargetSet(m_lightingTargetSet);

		m_directionalStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_deferredDirectionalLightShaderName, fullscreenQuadStageParams));

		m_directionalStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_directionalStage->AddPermanentBuffer(poissonSampleParams);

		pipeline.AppendRenderStage(m_directionalStage);


		// Point light stage:
		//-------------------
		m_pointStage->SetTextureTargetSet(m_lightingTargetSet);

		m_pointStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_pointStage->AddPermanentBuffer(poissonSampleParams);

		// Light volumes only illuminate something if the geometry is behind it
		re::PipelineState deferredMeshStageParams(fullscreenQuadStageParams);
		deferredMeshStageParams.SetDepthTestMode(re::PipelineState::DepthTestMode::GEqual);
		deferredMeshStageParams.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Front); // Cull front faces of light volumes

		m_pointStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_deferredPointLightShaderName, deferredMeshStageParams));

		pipeline.AppendRenderStage(m_pointStage);


		// Spot light stage:
		//------------------
		m_spotStage->SetTextureTargetSet(m_lightingTargetSet);

		m_spotStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_spotStage->AddPermanentBuffer(poissonSampleParams);

		m_spotStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_deferredSpotLightShaderName, deferredMeshStageParams));

		pipeline.AppendRenderStage(m_spotStage);


		// Attach GBuffer color inputs:
		std::shared_ptr<re::Sampler> wrapMinMagLinearMipPoint = re::Sampler::GetSampler("WrapMinMagLinearMipPoint");

		constexpr uint8_t numGBufferColorInputs = 
			static_cast<uint8_t>(GBufferGraphicsSystem::GBufferTexIdx::GBufferColorTex_Count);

		for (uint8_t slot = 0; slot < numGBufferColorInputs; slot++)
		{
			if (slot == GBufferGraphicsSystem::GBufferEmissive)
			{
				continue; // The emissive texture is not used
			}

			SEAssert(texDependencies.contains(GBufferGraphicsSystem::GBufferTexNames[slot]),
				"Texture dependency not found");

			char const* texName = GBufferGraphicsSystem::GBufferTexNames[slot];
			std::shared_ptr<re::Texture> const& gbufferTex = texDependencies.at(texName);
			
			m_ambientStage->AddTextureInput(texName, gbufferTex, wrapMinMagLinearMipPoint);
			m_directionalStage->AddTextureInput(texName, gbufferTex, wrapMinMagLinearMipPoint);
			m_pointStage->AddTextureInput(texName, gbufferTex, wrapMinMagLinearMipPoint);
			m_spotStage->AddTextureInput(texName, gbufferTex, wrapMinMagLinearMipPoint);
		}


		// Attach the GBUffer depth input:
		constexpr uint8_t depthBufferSlot = gr::GBufferGraphicsSystem::GBufferDepth;
		char const* depthName = GBufferGraphicsSystem::GBufferTexNames[depthBufferSlot];
		std::shared_ptr<re::Texture> const& depthTex = texDependencies.at(depthName);

		m_directionalStage->AddTextureInput(depthName, depthTex, wrapMinMagLinearMipPoint);
		m_pointStage->AddTextureInput(depthName, depthTex, wrapMinMagLinearMipPoint);
		m_spotStage->AddTextureInput(depthName, depthTex, wrapMinMagLinearMipPoint);
		m_ambientStage->AddTextureInput(depthName, depthTex, wrapMinMagLinearMipPoint);

		m_ambientStage->AddTextureInput("Tex7", m_BRDF_integrationMap, clampMinMagMipPoint);
	}


	void DeferredLightingGraphicsSystem::PreRender(DataDependencies const& dataDependencies)
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Removed any deleted directional/point/spot lights:
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
		if (renderData.HasIDsWithDeletedData<gr::Light::RenderDataSpot>())
		{
			DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataSpot>(), m_punctualLightData);
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

				const AmbientLightData ambientLightParamsData = GetAmbientLightParamsData(
					totalPMREMMipLevels,
					ambientData.m_diffuseScale,
					ambientData.m_specularScale,
					m_ssaoTex.get());

				std::shared_ptr<re::Buffer> ambientParams = re::Buffer::Create(
					AmbientLightData::s_shaderName,
					ambientLightParamsData,
					re::Buffer::Type::Mutable);

				m_ambientLightData.emplace(ambientData.m_renderDataID,
					AmbientLightRenderData{
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

				ambientBatch.SetBuffer(ambientParams);

				++ambientItr;
			}
		}

		using ShadowTextureMap = std::unordered_map<gr::RenderDataID, re::Texture const*>;
		
		ShadowTextureMap const* shadowTextures = 
			static_cast<ShadowTextureMap const*>(dataDependencies.at(k_shadowTexturesInput));
		
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

				LightData const& directionalLightParamData = GetLightParamData(
					&directionalData,
					gr::Light::Type::Directional,
					directionalTransformData,
					shadowData,
					shadowCamData,
					m_directionalStage->GetTextureTargetSet().get());

				std::shared_ptr<re::Buffer> directionalLightParams = re::Buffer::Create(
					LightData::s_shaderName,
					directionalLightParamData,
					re::Buffer::Type::Mutable);

				m_punctualLightData.emplace(
					directionalItr.GetRenderDataID(),
					PunctualLightRenderData{
						.m_type = gr::Light::Directional,
						.m_lightParams = directionalLightParams,
						.m_transformParams = nullptr,
						.m_batch = re::Batch(re::Batch::Lifetime::Permanent, meshData, nullptr)
					});

				re::Batch& directionalLightBatch = m_punctualLightData.at(directionalData.m_renderDataID).m_batch;

				directionalLightBatch.SetBuffer(directionalLightParams);

				if (directionalData.m_hasShadow) // Add the shadow map texture to the batch
				{
					re::Texture const* shadowTex = shadowTextures ? 
						shadowTextures->at(directionalData.m_renderDataID) : m_missing2DShadowFallback.get();

					directionalLightBatch.AddTextureAndSamplerInput(
						"Depth0",
						shadowTex,
						re::Sampler::GetSampler("BorderCmpMinMagLinearMipPoint"));
				}

				++directionalItr;
			}
		}


		auto RegisterNewDeferredMeshLight = [&](
			gr::RenderDataManager::IDIterator const& lightItr,
			gr::Light::Type lightType,
			void const* lightRenderData,
			bool hasShadow,
			re::TextureTargetSet const* stageTargetSet,
			std::unordered_map<gr::RenderDataID, PunctualLightRenderData>& punctualLightData,
			char const* depthInputTexName,
			char const* samplerTypeName)
			{
				gr::MeshPrimitive::RenderData const& meshData = lightItr.Get<gr::MeshPrimitive::RenderData>();
				gr::Transform::RenderData const& transformData = lightItr.GetTransformData();

				gr::ShadowMap::RenderData const* shadowData = nullptr;
				gr::Camera::RenderData const* shadowCamData = nullptr;
				if (hasShadow)
				{
					shadowData = &lightItr.Get<gr::ShadowMap::RenderData>();
					shadowCamData = &lightItr.Get<gr::Camera::RenderData>();
				}

				LightData const& lightData = GetLightParamData(
					lightRenderData,
					lightType,
					transformData,
					shadowData,
					shadowCamData,
					stageTargetSet);

				std::shared_ptr<re::Buffer> lightDataBuffer = re::Buffer::Create(
					LightData::s_shaderName,
					lightData,
					re::Buffer::Type::Mutable);

				std::shared_ptr<re::Buffer> transformBuffer = gr::Transform::CreateInstancedTransformBuffer(
					re::Buffer::Type::Mutable, transformData);

				punctualLightData.emplace(
					lightItr.GetRenderDataID(),
					PunctualLightRenderData{
						.m_type = lightType,
						.m_lightParams = lightDataBuffer,
						.m_transformParams = transformBuffer,
						.m_batch = re::Batch(re::Batch::Lifetime::Permanent, meshData, nullptr)
					});

				const gr::RenderDataID lightRenderDataID = lightItr.GetRenderDataID();

				re::Batch& lightBatch = punctualLightData.at(lightRenderDataID).m_batch;

				lightBatch.SetBuffer(lightDataBuffer);
				lightBatch.SetBuffer(transformBuffer);

				if (hasShadow) // Add the shadow map texture to the batch
				{
					re::Texture const* shadowTex = nullptr;
					if (shadowTextures)
					{
						shadowTex = shadowTextures->at(lightRenderDataID);
					}
					else
					{
						switch (lightType)
						{
						case gr::Light::Type::Point:
						{
							shadowTex = m_missingCubeShadowFallback.get();
						}
						break;
						case gr::Light::Type::Spot:
						{
							shadowTex = m_missing2DShadowFallback.get();
						}
						break;
						default: SEAssertF("Invalid/unexpected light type");
						}
					}

					lightBatch.AddTextureAndSamplerInput(
						depthInputTexName,
						shadowTex,
						re::Sampler::GetSampler(samplerTypeName));
				}
			};
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataPoint>())
		{
			std::vector<gr::RenderDataID> const& newPointIDs =
				renderData.GetIDsWithNewData<gr::Light::RenderDataPoint>();

			auto pointItr = renderData.IDBegin(newPointIDs);
			auto const& pointItrEnd = renderData.IDEnd(newPointIDs);
			while (pointItr != pointItrEnd)
			{
				gr::Light::RenderDataPoint const& pointData = pointItr.Get<gr::Light::RenderDataPoint>();
				const bool hasShadow = pointData.m_hasShadow;

				RegisterNewDeferredMeshLight(
					pointItr,
					gr::Light::Point, 
					&pointData, 
					hasShadow, 
					m_pointStage->GetTextureTargetSet().get(),
					m_punctualLightData,
					"CubeDepth",
					"WrapCmpMinMagLinearMipPoint");

				++pointItr;
			}
		}
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataSpot>())
		{
			std::vector<gr::RenderDataID> const& newSpotIDs = renderData.GetIDsWithNewData<gr::Light::RenderDataSpot>();

			auto spotItr = renderData.IDBegin(newSpotIDs);
			auto const& spotItrEnd = renderData.IDEnd(newSpotIDs);
			while (spotItr != spotItrEnd)
			{
				gr::Light::RenderDataSpot const& spotData = spotItr.Get<gr::Light::RenderDataSpot>();
				const bool hasShadow = spotData.m_hasShadow;

				RegisterNewDeferredMeshLight(
					spotItr,
					gr::Light::Spot,
					&spotData,
					hasShadow,
					m_spotStage->GetTextureTargetSet().get(),
					m_punctualLightData,
					"Depth0",
					"WrapCmpMinMagLinearMipPoint");

				++spotItr;
			}
		}

		CreateBatches(dataDependencies);
	}


	void DeferredLightingGraphicsSystem::CreateBatches(DataDependencies const& dataDependencies)
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

				const AmbientLightData ambientLightParamsData = GetAmbientLightParamsData(
					totalPMREMMipLevels,
					ambientRenderData.m_diffuseScale,
					ambientRenderData.m_specularScale,
					m_ssaoTex.get());

				ambientLight.second.m_ambientParams->Commit(ambientLightParamsData);
			}
		}

		if (m_graphicsSystemManager->HasActiveAmbientLight())
		{
			const gr::RenderDataID activeAmbientID = m_graphicsSystemManager->GetActiveAmbientLightID();

			SEAssert(m_ambientLightData.contains(activeAmbientID), "Cannot find active ambient light");

			m_ambientStage->AddBatch(m_ambientLightData.at(activeAmbientID).m_batch);
		}
		

		// Hash culled visible light IDs so we can quickly check if we need to add a point/spot light's batch:
		std::unordered_set<gr::RenderDataID> visibleLightIDs;

		auto MarkIDsVisible = [&](std::vector<gr::RenderDataID> const* lightIDs)
			{
				for (gr::RenderDataID lightID : *lightIDs)
				{
					visibleLightIDs.emplace(lightID);
				}
			};
		auto MarkAllIDsVisible = [&](auto& lightObjectItr, auto const& lightObjectItrEnd)
			{
				while (lightObjectItr != lightObjectItrEnd)
				{
					visibleLightIDs.emplace(lightObjectItr.GetRenderDataID());
					++lightObjectItr;
				}
			};

		PunctualLightCullingResults const* spotIDs = 
			static_cast<PunctualLightCullingResults const*>(dataDependencies.at(k_spotLightCullingInput));
		if (spotIDs)
		{
			MarkIDsVisible(spotIDs);
		}
		else
		{
			auto spotItr = renderData.ObjectBegin<gr::Light::RenderDataSpot>();
			auto const& spotItrEnd = renderData.ObjectEnd<gr::Light::RenderDataSpot>();
			MarkAllIDsVisible(spotItr, spotItrEnd);
		}

		PunctualLightCullingResults const* pointIDs = 
			static_cast<PunctualLightCullingResults const*>(dataDependencies.at(k_pointLightCullingInput));
		if (pointIDs)
		{
			MarkIDsVisible(pointIDs);
		}
		else
		{
			auto pointItr = renderData.ObjectBegin<gr::Light::RenderDataPoint>();
			auto const& pointItrEnd = renderData.ObjectEnd<gr::Light::RenderDataPoint>();
			MarkAllIDsVisible(pointItr, pointItrEnd);
		}


		// Update all of the punctual lights we're tracking:
		for (auto& light : m_punctualLightData)
		{
			const gr::RenderDataID lightID = light.first;

			// Update lighting buffers, if anything is dirty:
			const bool transformIsDirty = renderData.TransformIsDirtyFromRenderDataID(lightID);

			const bool lightRenderDataDirty = 
				(light.second.m_type == gr::Light::Type::Directional &&
					renderData.IsDirty<gr::Light::RenderDataDirectional>(lightID)) ||
				(light.second.m_type == gr::Light::Type::Point &&
					renderData.IsDirty<gr::Light::RenderDataPoint>(lightID)) ||
				(light.second.m_type == gr::Light::Type::Spot &&
					renderData.IsDirty<gr::Light::RenderDataSpot>(lightID));

			const bool shadowDataIsDirty = 
				(renderData.HasObjectData<gr::ShadowMap::RenderData>(lightID) && 
					renderData.IsDirty<gr::ShadowMap::RenderData>(lightID) ) ||
				(renderData.HasObjectData<gr::Camera::RenderData>(lightID) &&
					renderData.IsDirty<gr::Camera::RenderData>(lightID));

			if (transformIsDirty || lightRenderDataDirty || shadowDataIsDirty)
			{
				gr::Transform::RenderData const& lightTransformData =
					renderData.GetTransformDataFromRenderDataID(lightID);

				void const* lightRenderData = nullptr;
				gr::ShadowMap::RenderData const* shadowData = nullptr;
				gr::Camera::RenderData const* shadowCamData = nullptr;
				re::TextureTargetSet const* targetSet = nullptr;
				bool hasShadow = false;

				switch (light.second.m_type)
				{
				case gr::Light::Type::Directional:
				{
					gr::Light::RenderDataDirectional const& directionalData =
						renderData.GetObjectData<gr::Light::RenderDataDirectional>(lightID);
					light.second.m_canContribute = directionalData.m_canContribute;
					lightRenderData = &directionalData;
					hasShadow = directionalData.m_hasShadow;
					targetSet = m_directionalStage->GetTextureTargetSet().get();
				}
				break;
				case gr::Light::Type::Point:
				{
					gr::Light::RenderDataPoint const& pointData = 
						renderData.GetObjectData<gr::Light::RenderDataPoint>(lightID);
					light.second.m_canContribute = pointData.m_canContribute;
					lightRenderData = &pointData;
					hasShadow = pointData.m_hasShadow;
					targetSet = m_pointStage->GetTextureTargetSet().get();

					light.second.m_transformParams->Commit(gr::Transform::CreateInstancedTransformData(
							renderData.GetTransformDataFromRenderDataID(lightID)));
				}
				break;
				case gr::Light::Type::Spot:
				{
					gr::Light::RenderDataSpot const& spotData =
						renderData.GetObjectData<gr::Light::RenderDataSpot>(lightID);
					light.second.m_canContribute = spotData.m_canContribute;
					lightRenderData = &spotData;
					hasShadow = spotData.m_hasShadow;
					targetSet = m_spotStage->GetTextureTargetSet().get();

					light.second.m_transformParams->Commit(gr::Transform::CreateInstancedTransformData(
						renderData.GetTransformDataFromRenderDataID(lightID)));
				}
				break;
				default: SEAssertF("Invalid light type");
				}

				// Commit the light buffer data:
				if (hasShadow)
				{
					shadowData = &renderData.GetObjectData<gr::ShadowMap::RenderData>(lightID);
					shadowCamData = &renderData.GetObjectData<gr::Camera::RenderData>(lightID);
				}
				LightData const& lightParams = GetLightParamData(
					lightRenderData,
					light.second.m_type,
					lightTransformData,
					shadowData,
					shadowCamData,
					targetSet);
				light.second.m_lightParams->Commit(lightParams);
			}

			// Add punctual batches:
			if (light.second.m_canContribute)
			{
				switch (light.second.m_type)
				{
				case gr::Light::Type::Directional:
				{
					m_directionalStage->AddBatch(light.second.m_batch);
				}
				break;
				case gr::Light::Type::Point:
				{
					if (visibleLightIDs.contains(lightID))
					{
						m_pointStage->AddBatch(light.second.m_batch);
					}
				}
				break;
				case gr::Light::Type::Spot:
				{
					if (visibleLightIDs.contains(lightID))
					{
						m_spotStage->AddBatch(light.second.m_batch);
					}
				}
				break;
				case gr::Light::Type::AmbientIBL:
				default: SEAssertF("Invalid light type");
				}
			}
		}
	}
}