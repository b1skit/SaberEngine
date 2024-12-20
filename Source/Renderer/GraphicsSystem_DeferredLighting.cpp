// © 2022 Adam Badke. All rights reserved.
#include "Buffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystemManager.h"
#include "LightParamsHelpers.h"
#include "LightRenderData.h"
#include "MeshFactory.h"
#include "Sampler.h"
#include "ShadowMapRenderData.h"
#include "RenderDataManager.h"
#include "RenderManager.h"
#include "RenderStage.h"

#include "Core/Config.h"

#include "Shaders/Common/IBLGenerationParams.h"
#include "Shaders/Common/LightParams.h"


namespace
{
	static const EffectID k_deferredLightingEffectID = effect::Effect::ComputeEffectID("DeferredLighting");


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
		// Empirical testing shows that for N = 4096 IEM samples per pixel, this fudge factor gives reasonable results.
		// We assume our IBL inputs are roughly 2:1 in dimensions, and compute the src mip from the maximum dimension
		const float maxDimension = static_cast<float>(std::max(srcWidth, srcHeight));

		generationParams.g_mipLevelSrcWidthSrcHeightSrcNumMips = glm::vec4(
			glm::log2(glm::sqrt(maxDimension)),
			srcWidth, 
			srcHeight, 
			numMipLevels);

		return generationParams;
	}


	re::TextureView CreateShadowArrayReadView(core::InvPtr<re::Texture> const& shadowArray)
	{
		return re::TextureView(
			shadowArray,
			{ re::TextureView::ViewFlags::ReadOnlyDepth });
	}
}


namespace gr
{
	DeferredLightingGraphicsSystem::DeferredLightingGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_resourceCreationStagePipeline(nullptr)
		, m_pointCullingResults(nullptr)
		, m_spotCullingResults(nullptr)
		, m_directionalLightDataBuffer(nullptr)
		, m_pointLightDataBuffer(nullptr)
		, m_spotLightDataBuffer(nullptr)
		, m_directionalLightDataBufferIdxMap(nullptr)
		, m_pointLightDataBufferIdxMap(nullptr)
		, m_spotLightDataBufferIdxMap(nullptr)
		, m_directionalShadowArrayTex(nullptr)
		, m_pointShadowArrayTex(nullptr)
		, m_spotShadowArrayTex(nullptr)
		, m_directionalShadowArrayIdxMap(nullptr)
		, m_pointShadowArrayIdxMap(nullptr)
		, m_spotShadowArrayIdxMap(nullptr)
		, m_PCSSSampleParamsBuffer(nullptr)
	{
		m_lightingTargetSet = re::TextureTargetSet::Create("Deferred light targets");
	}


	void DeferredLightingGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_ssaoInput, TextureInputDefault::OpaqueWhite);

		RegisterDataInput(k_pointLightCullingDataInput);
		RegisterDataInput(k_spotLightCullingDataInput);

		RegisterBufferInput(k_directionalLightDataBufferInput);
		RegisterBufferInput(k_pointLightDataBufferInput);
		RegisterBufferInput(k_spotLightDataBufferInput);

		RegisterDataInput(k_IDToDirectionalIdxDataInput);
		RegisterDataInput(k_IDToPointIdxDataInput);
		RegisterDataInput(k_IDToSpotIdxDataInput);

		RegisterTextureInput(k_directionalShadowArrayTexInput);
		RegisterTextureInput(k_pointShadowArrayTexInput);
		RegisterTextureInput(k_spotShadowArrayTexInput);

		RegisterDataInput(k_IDToDirectionalShadowArrayIdxDataInput);
		RegisterDataInput(k_IDToPointShadowArrayIdxDataInput);
		RegisterDataInput(k_IDToSpotShadowArrayIdxDataInput);

		RegisterBufferInput(k_PCSSSampleParamsBufferInput);

		// Deferred lighting GS is (currently) tightly coupled to the GBuffer GS
		for (uint8_t slot = 0; slot < GBufferGraphicsSystem::GBufferColorTex_Count; slot++)
		{
			if (slot == GBufferGraphicsSystem::GBufferEmissive)
			{
				continue;
			}

			RegisterTextureInput(GBufferGraphicsSystem::GBufferTexNameHashKeys[slot]);
		}
		RegisterTextureInput(GBufferGraphicsSystem::GBufferTexNameHashKeys[GBufferGraphicsSystem::GBufferDepth]);
	}


	void DeferredLightingGraphicsSystem::RegisterOutputs()
	{
		RegisterTextureOutput(k_lightingTexOutput, &m_lightingTargetSet->GetColorTarget(0).GetTexture());
		RegisterTextureOutput(k_activeAmbientIEMTexOutput, &m_activeAmbientLightData.m_IEMTex);
		RegisterTextureOutput(k_activeAmbientPMREMTexOutput, &m_activeAmbientLightData.m_PMREMTex);
		RegisterTextureOutput(k_activeAmbientDFGTexOutput, &m_BRDF_integrationMap);
		
		RegisterBufferOutput(k_activeAmbientParamsBufferOutput, &m_activeAmbientLightData.m_ambientParams);
	}


	void DeferredLightingGraphicsSystem::CreateSingleFrameBRDFPreIntegrationStage(re::StagePipeline& pipeline)
	{
		re::RenderStage::ComputeStageParams computeStageParams;
		std::shared_ptr<re::RenderStage> brdfStage =
			re::RenderStage::CreateSingleFrameComputeStage("BRDF pre-integration compute stage", computeStageParams);

		brdfStage->SetDrawStyle(effect::drawstyle::DeferredLighting_BRDFIntegration);

		const uint32_t brdfTexWidthHeight =
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey));

		// Create a render target texture:			
		re::Texture::TextureParams brdfParams;
		brdfParams.m_width = brdfTexWidthHeight;
		brdfParams.m_height = brdfTexWidthHeight;
		brdfParams.m_usage =
			static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc);
		brdfParams.m_dimension = re::Texture::Dimension::Texture2D;
		brdfParams.m_format = re::Texture::Format::RGBA16F;
		brdfParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		brdfParams.m_mipMode = re::Texture::MipMode::None;
		brdfParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		m_BRDF_integrationMap = re::Texture::Create("BRDFIntegrationMap", brdfParams);

		brdfStage->AddSingleFrameRWTextureInput("output0", m_BRDF_integrationMap, re::TextureView::Texture2DView(0, 1));

		BRDFIntegrationData const& brdfIntegrationParams = GetBRDFIntegrationParamsDataData();
		std::shared_ptr<re::Buffer> brdfIntegrationBuf = re::Buffer::Create(
			BRDFIntegrationData::s_shaderName,
			brdfIntegrationParams,
			re::Buffer::BufferParams{
				.m_lifetime = re::Lifetime::SingleFrame,
				.m_stagingPool = re::Buffer::StagingPool::Temporary,
				.m_memPoolPreference = re::Buffer::UploadHeap,
				.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
				.m_usageMask = re::Buffer::Constant,
			});
		brdfStage->AddSingleFrameBuffer(BRDFIntegrationData::s_shaderName, brdfIntegrationBuf);

		// Add our dispatch information to a compute batch. Note: We use numthreads = (1,1,1)
		re::Batch computeBatch = re::Batch(
			re::Lifetime::SingleFrame,
			re::Batch::ComputeParams{
				.m_threadGroupCount = glm::uvec3(brdfTexWidthHeight, brdfTexWidthHeight, 1u) },
			k_deferredLightingEffectID);

		brdfStage->AddBatch(computeBatch);

		pipeline.AppendSingleFrameRenderStage(std::move(brdfStage));
	}


	void DeferredLightingGraphicsSystem::PopulateIEMTex(
		re::StagePipeline* pipeline, core::InvPtr<re::Texture> const& iblTex, core::InvPtr<re::Texture>& iemTexOut) const
	{
		const uint32_t iemTexWidthHeight =
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_iemTexWidthHeightKey));

		// IEM-specific texture params:
		re::Texture::TextureParams iemTexParams;
		iemTexParams.m_width = iemTexWidthHeight;
		iemTexParams.m_height = iemTexWidthHeight;
		iemTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc);
		iemTexParams.m_dimension = re::Texture::Dimension::TextureCube;
		iemTexParams.m_format = re::Texture::Format::RGBA16F;
		iemTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		iemTexParams.m_mipMode = re::Texture::MipMode::None;

		const std::string IEMTextureName = iblTex->GetName() + "_IEMTexture";
		iemTexOut = re::Texture::Create(IEMTextureName, iemTexParams);

		for (uint32_t face = 0; face < 6; face++)
		{
			re::RenderStage::GraphicsStageParams gfxStageParams;
			std::shared_ptr<re::RenderStage> iemStage = re::RenderStage::CreateSingleFrameGraphicsStage(
				std::format("IEM generation: Face {}/6", face + 1).c_str(), gfxStageParams);

			iemStage->SetDrawStyle(effect::drawstyle::DeferredLighting_IEMGeneration);
			iemStage->AddPermanentTextureInput(
				"Tex0",
				iblTex,
				re::Sampler::GetSampler("WrapMinMagLinearMipPoint"),
				re::TextureView(iblTex));

			// Buffers:
			IEMPMREMGenerationData const& iemGenerationParams =
				GetIEMPMREMGenerationParamsDataData(0, 1, face, iblTex->Width(), iblTex->Height());
			std::shared_ptr<re::Buffer> iemGenerationBuffer = re::Buffer::Create(
				IEMPMREMGenerationData::s_shaderName,
				iemGenerationParams,
				re::Buffer::BufferParams{
					.m_lifetime = re::Lifetime::SingleFrame,
					.m_stagingPool = re::Buffer::StagingPool::Temporary,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Constant,
				});
			iemStage->AddSingleFrameBuffer(IEMPMREMGenerationData::s_shaderName, iemGenerationBuffer);

			iemStage->AddPermanentBuffer(CameraData::s_shaderName, m_cubemapRenderCamParams[face]);

			std::shared_ptr<re::TextureTargetSet> iemTargets = re::TextureTargetSet::Create("IEM Stage Targets");

			iemTargets->SetColorTarget(
				0,
				iemTexOut, 
				re::TextureTarget::TargetParams{.m_textureView = re::TextureView::Texture2DArrayView(0, 1, face, 1) });
			iemTargets->SetViewport(re::Viewport(0, 0, iemTexWidthHeight, iemTexWidthHeight));
			iemTargets->SetScissorRect(re::ScissorRect(0, 0, iemTexWidthHeight, iemTexWidthHeight));

			iemStage->SetTextureTargetSet(iemTargets);

			iemStage->AddBatch(*m_cubeMeshBatch);

			pipeline->AppendSingleFrameRenderStage(std::move(iemStage));
		}
	}


	void DeferredLightingGraphicsSystem::PopulatePMREMTex(
		re::StagePipeline* pipeline, core::InvPtr<re::Texture> const& iblTex, core::InvPtr<re::Texture>& pmremTexOut) const
	{
		const uint32_t pmremTexWidthHeight =
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_pmremTexWidthHeightKey));

		// PMREM-specific texture params:
		re::Texture::TextureParams pmremTexParams;
		pmremTexParams.m_width = pmremTexWidthHeight;
		pmremTexParams.m_height = pmremTexWidthHeight;
		pmremTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc);
		pmremTexParams.m_dimension = re::Texture::Dimension::TextureCube;
		pmremTexParams.m_format = re::Texture::Format::RGBA16F;
		pmremTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		pmremTexParams.m_createAsPermanent = false;
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

				pmremStage->SetDrawStyle(effect::drawstyle::DeferredLighting_PMREMGeneration);

				pmremStage->AddPermanentTextureInput(
					"Tex0",
					iblTex,
					re::Sampler::GetSampler("ClampMinMagMipLinear"),
					re::TextureView(iblTex));

				// Buffers:
				IEMPMREMGenerationData const& pmremGenerationParams = GetIEMPMREMGenerationParamsDataData(
					currentMipLevel, totalMipLevels, face, iblTex->Width(), iblTex->Height());
				std::shared_ptr<re::Buffer> pmremGenerationBuffer = re::Buffer::Create(
					IEMPMREMGenerationData::s_shaderName,
					pmremGenerationParams,
					re::Buffer::BufferParams{
						.m_lifetime = re::Lifetime::SingleFrame,
						.m_stagingPool = re::Buffer::StagingPool::Temporary,
						.m_memPoolPreference = re::Buffer::UploadHeap,
						.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
						.m_usageMask = re::Buffer::Constant,
					});
				pmremStage->AddSingleFrameBuffer(IEMPMREMGenerationData::s_shaderName, pmremGenerationBuffer);

				pmremStage->AddPermanentBuffer(CameraData::s_shaderName, m_cubemapRenderCamParams[face]);

				std::shared_ptr<re::TextureTargetSet> pmremTargetSet =
					re::TextureTargetSet::Create("PMREM texture targets: Face " + postFix);

				pmremTargetSet->SetColorTarget(
					0,
					pmremTexOut, 
					re::TextureTarget::TargetParams{
						.m_textureView = re::TextureView::Texture2DArrayView(currentMipLevel, 1, face, 1) });

				const glm::vec4 mipDimensions =
					pmremTexOut->GetMipLevelDimensions(currentMipLevel);
				const uint32_t mipWidth = static_cast<uint32_t>(mipDimensions.x);
				const uint32_t mipHeight = static_cast<uint32_t>(mipDimensions.y);

				pmremTargetSet->SetViewport(re::Viewport(0, 0, mipWidth, mipHeight));
				pmremTargetSet->SetScissorRect(re::ScissorRect(0, 0, mipWidth, mipHeight));

				pmremStage->SetTextureTargetSet(pmremTargetSet);

				pmremStage->AddBatch(*m_cubeMeshBatch);

				pipeline->AppendSingleFrameRenderStage(std::move(pmremStage));
			}
		}
	}


	void DeferredLightingGraphicsSystem::InitializeResourceGenerationStages(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		m_resourceCreationStagePipeline = &pipeline;

		m_resourceCreationStageParentItr = pipeline.AppendRenderStage(
			re::RenderStage::CreateParentStage("Resource creation stages parent"));


		// Cube mesh, for rendering of IBL cubemaps
		if (m_cubeMeshPrimitive == nullptr)
		{
			m_cubeMeshPrimitive = gr::meshfactory::CreateCube(gr::meshfactory::FactoryOptions{.m_queueBufferCreation = false});
		}

		// Create a cube mesh batch, for reuse during the initial frame IBL rendering:
		if (m_cubeMeshBatch == nullptr)
		{
			m_cubeMeshBatch = std::make_unique<re::Batch>(
				re::Lifetime::Permanent,
				m_cubeMeshPrimitive.get(),
				k_deferredLightingEffectID);
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
					re::Buffer::BufferParams{
						.m_stagingPool = re::Buffer::StagingPool::Temporary,
						.m_memPoolPreference = re::Buffer::UploadHeap,
						.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
						.m_usageMask = re::Buffer::Constant,
					});
			}
		}

		// 1st frame: Generate the pre-integrated BRDF LUT via a single-frame compute stage:
		CreateSingleFrameBRDFPreIntegrationStage(pipeline);
	}


	void DeferredLightingGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const& bufferDependencies,
		DataDependencies const& dataDependencies)
	{
		// Cache our dependencies:
		m_pointCullingResults = GetDataDependency<PunctualLightCullingResults>(k_pointLightCullingDataInput, dataDependencies);
		m_spotCullingResults = GetDataDependency<PunctualLightCullingResults>(k_spotLightCullingDataInput, dataDependencies);

		m_directionalLightDataBuffer = bufferDependencies.at(k_directionalLightDataBufferInput);
		m_pointLightDataBuffer = bufferDependencies.at(k_pointLightDataBufferInput);
		m_spotLightDataBuffer = bufferDependencies.at(k_spotLightDataBufferInput);

		m_directionalLightDataBufferIdxMap = GetDataDependency<LightDataBufferIdxMap>(k_IDToDirectionalIdxDataInput, dataDependencies);
		m_pointLightDataBufferIdxMap = GetDataDependency<LightDataBufferIdxMap>(k_IDToPointIdxDataInput, dataDependencies);
		m_spotLightDataBufferIdxMap = GetDataDependency<LightDataBufferIdxMap>(k_IDToSpotIdxDataInput, dataDependencies);

		m_directionalShadowArrayTex = texDependencies.at(k_directionalShadowArrayTexInput);
		m_pointShadowArrayTex = texDependencies.at(k_pointShadowArrayTexInput);
		m_spotShadowArrayTex = texDependencies.at(k_spotShadowArrayTexInput);

		m_directionalShadowArrayIdxMap = GetDataDependency<ShadowArrayIdxMap>(k_IDToDirectionalShadowArrayIdxDataInput, dataDependencies);
		m_pointShadowArrayIdxMap = GetDataDependency<ShadowArrayIdxMap>(k_IDToPointShadowArrayIdxDataInput, dataDependencies);
		m_spotShadowArrayIdxMap = GetDataDependency<ShadowArrayIdxMap>(k_IDToSpotShadowArrayIdxDataInput, dataDependencies);

		m_PCSSSampleParamsBuffer = bufferDependencies.at(k_PCSSSampleParamsBufferInput);


		m_missing2DShadowFallback = re::Texture::Create("Missing 2D shadow fallback",
			re::Texture::TextureParams
			{
				.m_usage = re::Texture::Usage::ColorSrc,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = re::Texture::Format::Depth32F,
				.m_colorSpace = re::Texture::ColorSpace::Linear,
				.m_mipMode = re::Texture::MipMode::None,
				.m_clear = re::Texture::TextureParams::ClearValues{},
			},
			glm::vec4(1.f, 1.f, 1.f, 1.f));

		m_missingCubeShadowFallback = re::Texture::Create("Missing cubemap shadow fallback", 
			re::Texture::TextureParams
			{
				.m_usage = re::Texture::Usage::ColorSrc,
				.m_dimension = re::Texture::Dimension::TextureCube,
				.m_format = re::Texture::Format::Depth32F,
				.m_colorSpace = re::Texture::ColorSpace::Linear,
				.m_mipMode = re::Texture::MipMode::None,
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
		lightTargetTexParams.m_usage = 
			static_cast<re::Texture::Usage>(re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc);
		lightTargetTexParams.m_dimension = re::Texture::Dimension::Texture2D;
		lightTargetTexParams.m_format = re::Texture::Format::RGBA16F;
		lightTargetTexParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		lightTargetTexParams.m_mipMode = re::Texture::MipMode::None;
		lightTargetTexParams.m_clear.m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		core::InvPtr<re::Texture> const& lightTargetTex = re::Texture::Create("DeferredLightTarget", lightTargetTexParams);

		// Create the lighting target set (shared by all lights/stages):
		re::TextureTarget::TargetParams deferredTargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1)};
		deferredTargetParams.m_clearMode = re::TextureTarget::ClearMode::Disabled;

		m_lightingTargetSet->SetColorTarget(0, lightTargetTex, deferredTargetParams);

		// We need the depth buffer attached, but with depth writes disabled:
		re::TextureTarget::TargetParams depthTargetParams{ .m_textureView = {
				re::TextureView::Texture2DView(0, 1),
				{re::TextureView::ViewFlags::ReadOnlyDepth} } };

		m_lightingTargetSet->SetDepthStencilTarget(
			*texDependencies.at(GBufferGraphicsSystem::GBufferTexNameHashKeys[GBufferGraphicsSystem::GBufferDepth]),
			depthTargetParams);

		// Append a color-only clear stage to clear the lighting target:
		re::RenderStage::ClearStageParams colorClearParams;
		colorClearParams.m_colorClearModes = { re::TextureTarget::ClearMode::Enabled };
		colorClearParams.m_depthClearMode = re::TextureTarget::ClearMode::Disabled;
		pipeline.AppendRenderStage(re::RenderStage::CreateClearStage(colorClearParams, m_lightingTargetSet));


		// Ambient stage:
		// --------------
		m_ambientStage->SetTextureTargetSet(m_lightingTargetSet);

		m_ambientStage->SetDrawStyle(effect::drawstyle::DeferredLighting_DeferredAmbient);

		m_ambientStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());	

		// Get/set the AO texture. If it doesn't exist, we'll get a default opaque white texture
		m_ssaoTex = *texDependencies.at(k_ssaoInput);

		core::InvPtr<re::Sampler> const& clampMinMagMipPoint = re::Sampler::GetSampler("ClampMinMagMipPoint");

		m_ambientStage->AddPermanentTextureInput(
			k_ssaoInput.GetKey(), m_ssaoTex, clampMinMagMipPoint, re::TextureView(m_ssaoTex));

		// Append the ambient stage:
		pipeline.AppendRenderStage(m_ambientStage);
		

		// Directional light stage:
		//-------------------------
		m_directionalStage->SetTextureTargetSet(m_lightingTargetSet);
		
		m_directionalStage->SetDrawStyle(effect::drawstyle::DeferredLighting_DeferredDirectional);

		m_directionalStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_directionalStage->AddPermanentBuffer(PoissonSampleParamsData::s_shaderName, *m_PCSSSampleParamsBuffer);

		pipeline.AppendRenderStage(m_directionalStage);


		// Point light stage:
		//-------------------
		m_pointStage->SetTextureTargetSet(m_lightingTargetSet);
		m_pointStage->AddPermanentBuffer(m_lightingTargetSet->GetCreateTargetParamsBuffer());

		m_pointStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_pointStage->AddPermanentBuffer(PoissonSampleParamsData::s_shaderName, *m_PCSSSampleParamsBuffer);

		m_pointStage->SetDrawStyle(effect::drawstyle::DeferredLighting_DeferredPoint);

		pipeline.AppendRenderStage(m_pointStage);


		// Spot light stage:
		//------------------
		m_spotStage->SetTextureTargetSet(m_lightingTargetSet);
		m_spotStage->AddPermanentBuffer(m_lightingTargetSet->GetCreateTargetParamsBuffer());

		m_spotStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_spotStage->AddPermanentBuffer(PoissonSampleParamsData::s_shaderName, *m_PCSSSampleParamsBuffer);

		m_spotStage->SetDrawStyle(effect::drawstyle::DeferredLighting_DeferredSpot);

		pipeline.AppendRenderStage(m_spotStage);


		// Attach GBuffer color inputs:
		core::InvPtr<re::Sampler> const& wrapMinMagLinearMipPoint = re::Sampler::GetSampler("WrapMinMagLinearMipPoint");

		constexpr uint8_t numGBufferColorInputs = 
			static_cast<uint8_t>(GBufferGraphicsSystem::GBufferTexIdx::GBufferColorTex_Count);

		for (uint8_t slot = 0; slot < numGBufferColorInputs; slot++)
		{
			if (slot == GBufferGraphicsSystem::GBufferEmissive)
			{
				continue; // The emissive texture is not used
			}

			SEAssert(texDependencies.contains(GBufferGraphicsSystem::GBufferTexNameHashKeys[slot]),
				"Texture dependency not found");

			util::HashKey const& texName = GBufferGraphicsSystem::GBufferTexNameHashKeys[slot];
			core::InvPtr<re::Texture> const& gbufferTex = *texDependencies.at(texName);
			
			re::TextureView gbufferTexView = re::TextureView(gbufferTex);

			m_ambientStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, gbufferTexView);
			m_directionalStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, gbufferTexView);
			m_pointStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, gbufferTexView);
			m_spotStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, gbufferTexView);
		}


		// Attach the GBUffer depth input:
		constexpr uint8_t depthBufferSlot = gr::GBufferGraphicsSystem::GBufferDepth;
		util::HashKey const& depthName = GBufferGraphicsSystem::GBufferTexNameHashKeys[depthBufferSlot];
		core::InvPtr<re::Texture> const& depthTex = *texDependencies.at(depthName);

		const re::TextureView gbufferDepthTexView = re::TextureView(depthTex);

		m_directionalStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, gbufferDepthTexView);
		m_pointStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, gbufferDepthTexView);
		m_spotStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, gbufferDepthTexView);
		m_ambientStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, gbufferDepthTexView);

		m_ambientStage->AddPermanentTextureInput(
			"DFG", m_BRDF_integrationMap, clampMinMagMipPoint, re::TextureView(m_BRDF_integrationMap));
	}


	void DeferredLightingGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Removed any deleted directional/point/spot lights:
		auto DeleteLights = []<typename T>(
			std::vector<gr::RenderDataID> const* deletedIDs, std::unordered_map<gr::RenderDataID, T>&stageData)
		{
			if (!deletedIDs)
			{
				return;
			}
			for (gr::RenderDataID id : *deletedIDs)
			{
				stageData.erase(id);
			}
		};

		// Null out the active ambient light tracking if it has been deleted
		std::vector<gr::RenderDataID> const* deletedAmbientIDs =
			renderData.GetIDsWithDeletedData<gr::Light::RenderDataAmbientIBL>();
		for (auto const& deletedAmbientID : *deletedAmbientIDs)
		{
			if (deletedAmbientID == m_activeAmbientLightData.m_renderDataID)
			{
				m_activeAmbientLightData = {};
				break;
			}
		}
		DeleteLights(deletedAmbientIDs, m_ambientLightData);
		
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataDirectional>(), m_punctualLightData);
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataPoint>(), m_punctualLightData);
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataSpot>(), m_punctualLightData);
		

		// Register new ambient lights:
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataAmbientIBL>())
		{
			std::vector<gr::RenderDataID> const* newAmbientIDs =
				renderData.GetIDsWithNewData<gr::Light::RenderDataAmbientIBL>();

			if (newAmbientIDs)
			{
				auto ambientItr = renderData.IDBegin(*newAmbientIDs);
				auto const& ambientItrEnd = renderData.IDEnd(*newAmbientIDs);
				while (ambientItr != ambientItrEnd)
				{
					gr::Light::RenderDataAmbientIBL const& ambientData = ambientItr.Get<gr::Light::RenderDataAmbientIBL>();

					const gr::RenderDataID lightID = ambientData.m_renderDataID;

					gr::MeshPrimitive::RenderData const& ambientMeshPrimData =
						ambientItr.Get<gr::MeshPrimitive::RenderData>();

					core::InvPtr<re::Texture> const& iblTex = ambientData.m_iblTex;
					SEAssert(iblTex, "IBL texture cannot be null");

					core::InvPtr<re::Texture> iemTex;
					PopulateIEMTex(m_resourceCreationStagePipeline, iblTex, iemTex);

					core::InvPtr<re::Texture> pmremTex;
					PopulatePMREMTex(m_resourceCreationStagePipeline, iblTex, pmremTex);

					const uint32_t totalPMREMMipLevels = pmremTex->GetNumMips();

					const AmbientLightData ambientLightParamsData = GetAmbientLightParamsData(
						totalPMREMMipLevels,
						ambientData.m_diffuseScale,
						ambientData.m_specularScale,
						static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey)),
						m_ssaoTex);

					std::shared_ptr<re::Buffer> ambientParams = re::Buffer::Create(
						AmbientLightData::s_shaderName,
						ambientLightParamsData,
						re::Buffer::BufferParams{
							.m_stagingPool = re::Buffer::StagingPool::Permanent,
							.m_memPoolPreference = re::Buffer::UploadHeap,
							.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
							.m_usageMask = re::Buffer::Constant,
						});

					m_ambientLightData.emplace(ambientData.m_renderDataID,
						AmbientLightRenderData{
							.m_ambientParams = ambientParams,
							.m_IEMTex = iemTex,
							.m_PMREMTex = pmremTex,
							.m_batch = re::Batch(re::Lifetime::Permanent, ambientMeshPrimData, nullptr)
						});

					// Set the batch inputs:
					re::Batch& ambientBatch = m_ambientLightData.at(lightID).m_batch;

					ambientBatch.SetEffectID(k_deferredLightingEffectID);

					ambientBatch.AddTextureInput(
						"CubeMapIEM",
						iemTex,
						re::Sampler::GetSampler("WrapMinMagMipLinear"),
						re::TextureView(iemTex));

					ambientBatch.AddTextureInput(
						"CubeMapPMREM",
						pmremTex,
						re::Sampler::GetSampler("WrapMinMagMipLinear"),
						re::TextureView(pmremTex));

					ambientBatch.SetBuffer(AmbientLightData::s_shaderName, ambientParams);

					++ambientItr;
				}
			}
		}

		// Update the params of the ambient lights we're tracking:
		for (auto& ambientLight : m_ambientLightData)
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
					static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey)),
					m_ssaoTex);

				ambientLight.second.m_ambientParams->Commit(ambientLightParamsData);
			}
		}

		// Update the shared active ambient light pointers:
		if (m_graphicsSystemManager->HasActiveAmbientLight() && 
			m_graphicsSystemManager->GetActiveAmbientLightID() != m_activeAmbientLightData.m_renderDataID)
		{
			const gr::RenderDataID activeAmbientID = m_graphicsSystemManager->GetActiveAmbientLightID();

			SEAssert(m_ambientLightData.contains(activeAmbientID), "Cannot find active ambient light");

			AmbientLightRenderData& activeAmbientLightData = m_ambientLightData.at(activeAmbientID);

			m_activeAmbientLightData.m_renderDataID = activeAmbientID;
			m_activeAmbientLightData.m_ambientParams = activeAmbientLightData.m_ambientParams;
			m_activeAmbientLightData.m_IEMTex = activeAmbientLightData.m_IEMTex;
			m_activeAmbientLightData.m_PMREMTex = activeAmbientLightData.m_PMREMTex;
		}
		
		// Register new directional lights:
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> const* newDirectionalIDs =
				renderData.GetIDsWithNewData<gr::Light::RenderDataDirectional>();
			if (newDirectionalIDs)
			{
				auto directionalItr = renderData.IDBegin(*newDirectionalIDs);
				auto const& directionalItrEnd = renderData.IDEnd(*newDirectionalIDs);
				while (directionalItr != directionalItrEnd)
				{
					gr::Light::RenderDataDirectional const& directionalData =
						directionalItr.Get<gr::Light::RenderDataDirectional>();

					gr::Transform::RenderData const& directionalTransformData = directionalItr.GetTransformData();
					gr::MeshPrimitive::RenderData const& meshData = directionalItr.Get<gr::MeshPrimitive::RenderData>();

					const gr::RenderDataID lightID = directionalItr.GetRenderDataID();

					m_punctualLightData.emplace(
						lightID,
						PunctualLightRenderData{
							.m_type = gr::Light::Directional,
							.m_transformParams = re::BufferInput(),
							.m_batch = re::Batch(re::Lifetime::Permanent, meshData, nullptr),
							.m_hasShadow = directionalData.m_hasShadow
						});

					re::Batch& directionalLightBatch = m_punctualLightData.at(directionalData.m_renderDataID).m_batch;

					directionalLightBatch.SetEffectID(k_deferredLightingEffectID);
					
					// Note: We set the shadow texture inputs per frame/batch if/as required

					++directionalItr;
				}
			}
		}


		auto RegisterNewDeferredMeshLight = [&](
			gr::RenderDataManager::IDIterator<std::vector<gr::RenderDataID>> const& lightItr,
			gr::Light::Type lightType,
			void const* lightRenderData,
			bool hasShadow,
			std::unordered_map<gr::RenderDataID, PunctualLightRenderData>& punctualLightData)
			{
				gr::MeshPrimitive::RenderData const& meshData = lightItr.Get<gr::MeshPrimitive::RenderData>();
				gr::Transform::RenderData const& transformData = lightItr.GetTransformData();

				re::BufferInput const& transformBuffer = gr::Transform::CreateInstancedTransformBufferInput(
					InstancedTransformData::s_shaderName, 
					re::Lifetime::Permanent, 
					re::Buffer::StagingPool::Permanent, 
					transformData);

				punctualLightData.emplace(
					lightItr.GetRenderDataID(),
					PunctualLightRenderData{
						.m_type = lightType,
						.m_transformParams = transformBuffer,
						.m_batch = re::Batch(re::Lifetime::Permanent, meshData, nullptr),
						.m_hasShadow = hasShadow
					});

				const gr::RenderDataID lightID = lightItr.GetRenderDataID();

				re::Batch& lightBatch = punctualLightData.at(lightID).m_batch;

				lightBatch.SetEffectID(k_deferredLightingEffectID);

				lightBatch.SetBuffer(transformBuffer);
				
				// Note: We set the shadow texture inputs per frame/batch if/as required
			};
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataPoint>())
		{
			std::vector<gr::RenderDataID> const* newPointIDs = renderData.GetIDsWithNewData<gr::Light::RenderDataPoint>();
			if (newPointIDs)
			{
				auto pointItr = renderData.IDBegin(*newPointIDs);
				auto const& pointItrEnd = renderData.IDEnd(*newPointIDs);
				while (pointItr != pointItrEnd)
				{
					gr::Light::RenderDataPoint const& pointData = pointItr.Get<gr::Light::RenderDataPoint>();
					const bool hasShadow = pointData.m_hasShadow;

					RegisterNewDeferredMeshLight(pointItr, gr::Light::Point, &pointData, hasShadow, m_punctualLightData);

					++pointItr;
				}
			}
		}
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataSpot>())
		{
			std::vector<gr::RenderDataID> const* newSpotIDs = renderData.GetIDsWithNewData<gr::Light::RenderDataSpot>();

			if (newSpotIDs)
			{
				auto spotItr = renderData.IDBegin(*newSpotIDs);
				auto const& spotItrEnd = renderData.IDEnd(*newSpotIDs);
				while (spotItr != spotItrEnd)
				{
					gr::Light::RenderDataSpot const& spotData = spotItr.Get<gr::Light::RenderDataSpot>();
					const bool hasShadow = spotData.m_hasShadow;

					RegisterNewDeferredMeshLight(spotItr, gr::Light::Spot, &spotData, hasShadow, m_punctualLightData);

					++spotItr;
				}
			}
		}

		// Attach the single-frame monolithic light data buffers:	
		m_directionalStage->AddSingleFrameBuffer(LightData::s_directionalLightDataShaderName, *m_directionalLightDataBuffer);
		m_pointStage->AddSingleFrameBuffer(LightData::s_pointLightDataShaderName, *m_pointLightDataBuffer);
		m_spotStage->AddSingleFrameBuffer(LightData::s_spotLightDataShaderName, *m_spotLightDataBuffer);

		CreateBatches();
	}


	void DeferredLightingGraphicsSystem::CreateBatches()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		if (m_activeAmbientLightData.m_renderDataID != gr::k_invalidRenderDataID)
		{
			SEAssert(m_ambientLightData.contains(m_activeAmbientLightData.m_renderDataID),
				"Cannot find active ambient light");

			m_ambientStage->AddBatch(m_ambientLightData.at(m_activeAmbientLightData.m_renderDataID).m_batch);
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

		if (m_spotCullingResults)
		{
			MarkIDsVisible(m_spotCullingResults);
		}
		else
		{
			auto spotItr = renderData.ObjectBegin<gr::Light::RenderDataSpot>();
			auto const& spotItrEnd = renderData.ObjectEnd<gr::Light::RenderDataSpot>();
			MarkAllIDsVisible(spotItr, spotItrEnd);
		}

		if (m_pointCullingResults)
		{
			MarkIDsVisible(m_pointCullingResults);
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
				}
				break;
				case gr::Light::Type::Point:
				{
					gr::Light::RenderDataPoint const& pointData = 
						renderData.GetObjectData<gr::Light::RenderDataPoint>(lightID);
					light.second.m_canContribute = pointData.m_canContribute;
					lightRenderData = &pointData;
					hasShadow = pointData.m_hasShadow;

					light.second.m_transformParams.GetBuffer()->Commit(gr::Transform::CreateInstancedTransformData(
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

					light.second.m_transformParams.GetBuffer()->Commit(gr::Transform::CreateInstancedTransformData(
						renderData.GetTransformDataFromRenderDataID(lightID)));
				}
				break;
				default: SEAssertF("Invalid light type");
				}
			}


			// Add punctual batches:
			if (light.second.m_canContribute &&
				(light.second.m_type == gr::Light::Type::Directional || 
					visibleLightIDs.contains(lightID)))
			{
				auto AddDuplicatedBatch = [&light, &lightID](
					re::RenderStage* stage,
					char const* shadowTexShaderName,
					util::StringHash const& samplerTypeName,
					LightDataBufferIdxMap const* lightDataBufferIdxMap,
					core::InvPtr<re::Texture> const* shadowArrayTex,
					ShadowArrayIdxMap const* shadowArrayIdxMap)
					{
						re::Batch* duplicatedBatch =
							stage->AddBatchWithLifetime(light.second.m_batch, re::Lifetime::SingleFrame);

						const uint32_t lightIdx = GetLightDataBufferIdx(lightDataBufferIdxMap, lightID);

						uint32_t shadowIdx = gr::k_invalidShadowIndex;
						if (light.second.m_hasShadow)
						{
							shadowIdx = GetShadowArrayIdx(shadowArrayIdxMap, lightID);

							// Note: Shadow array textures may be reallocated at the beginning of any frame; Texture
							// inputs/views must be re-set each frame (TODO: Skip recreating the views by tracking 
							// texture changes)						
							duplicatedBatch->AddTextureInput(
								shadowTexShaderName,
								*shadowArrayTex,
								re::Sampler::GetSampler(samplerTypeName),
								CreateShadowArrayReadView(*shadowArrayTex));
						}
						
						// Deferred light volumes: Single-frame buffer containing the indexes of a single light
						duplicatedBatch->SetBuffer(re::BufferInput(
							LightIndexData::s_shaderName,
							re::Buffer::Create(
								LightIndexData::s_shaderName,
								GetLightIndexData(lightIdx, shadowIdx),
								re::Buffer::BufferParams{
									.m_lifetime = re::Lifetime::SingleFrame,
									.m_stagingPool = re::Buffer::StagingPool::Temporary,
									.m_memPoolPreference = re::Buffer::UploadHeap,
									.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
									.m_usageMask = re::Buffer::Constant,
								})));
					};

				const util::StringHash sampler2DShadowName("BorderCmpMinMagLinearMipPoint");
				const util::StringHash samplerCubeShadowName("WrapCmpMinMagLinearMipPoint");
				switch (light.second.m_type)
				{
				case gr::Light::Type::Directional:
				{
					AddDuplicatedBatch(
						m_directionalStage.get(),
						"DirectionalShadows",
						sampler2DShadowName,
						m_directionalLightDataBufferIdxMap,
						m_directionalShadowArrayTex,
						m_directionalShadowArrayIdxMap);
				}
				break;
				case gr::Light::Type::Point:
				{
					AddDuplicatedBatch(
						m_pointStage.get(),
						"PointShadows",
						samplerCubeShadowName,
						m_pointLightDataBufferIdxMap,
						m_pointShadowArrayTex,
						m_pointShadowArrayIdxMap);
				}
				break;
				case gr::Light::Type::Spot:
				{
					AddDuplicatedBatch(
						m_spotStage.get(), 
						"SpotShadows", 
						sampler2DShadowName, 
						m_spotLightDataBufferIdxMap, 
						m_spotShadowArrayTex,
						m_spotShadowArrayIdxMap);
				}
				break;
				case gr::Light::Type::AmbientIBL:
				default: SEAssertF("Invalid light type");
				}
			}
		}
	}
}