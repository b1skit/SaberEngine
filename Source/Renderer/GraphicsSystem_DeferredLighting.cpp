// © 2022 Adam Badke. All rights reserved.
#include "BatchBuilder.h"
#include "BatchFactories.h"
#include "Buffer.h"
#include "IndexedBuffer.h"
#include "GraphicsSystem_DeferredLighting.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystemManager.h"
#include "GraphicsUtils.h"
#include "LightParamsHelpers.h"
#include "LightRenderData.h"
#include "MeshFactory.h"
#include "Sampler.h"
#include "RenderDataManager.h"
#include "RenderManager.h"
#include "Stage.h"

#include "Core/Config.h"

#include "Renderer/Shaders/Common/IBLGenerationParams.h"
#include "Renderer/Shaders/Common/InstancingParams.h"
#include "Renderer/Shaders/Common/LightParams.h"
#include "Renderer/Shaders/Common/ShadowParams.h"
#include "Renderer/Shaders/Common/TransformParams.h"


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
		, m_lightIDToShadowRecords(nullptr)
		, m_PCSSSampleParamsBuffer(nullptr)
	{
		m_lightingTargetSet = re::TextureTargetSet::Create("Deferred light targets");
	}


	void DeferredLightingGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_ssaoInput, TextureInputDefault::OpaqueWhite);

		RegisterDataInput(k_pointLightCullingDataInput);
		RegisterDataInput(k_spotLightCullingDataInput);

		RegisterDataInput(k_lightIDToShadowRecordInput);
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
		re::Stage::ComputeStageParams computeStageParams;
		std::shared_ptr<re::Stage> brdfStage =
			re::Stage::CreateSingleFrameComputeStage("BRDF pre-integration compute stage", computeStageParams);

		brdfStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_BRDFIntegration);

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
		
		const uint32_t dispatchXYDims = 
			grutil::GetRoundedDispatchDimension(brdfTexWidthHeight, BRDF_INTEGRATION_DISPATCH_XY_DIMS);

		brdfStage->AddBatch(gr::ComputeBatchBuilder()
			.SetThreadGroupCount(glm::uvec3(
				dispatchXYDims,
				dispatchXYDims,
				1u))
			.SetEffectID(k_deferredLightingEffectID)
			.BuildSingleFrame());

		pipeline.AppendSingleFrameStage(std::move(brdfStage));
	}


	void DeferredLightingGraphicsSystem::PopulateIEMTex(
		re::StagePipeline* pipeline, core::InvPtr<re::Texture> const& iblTex, core::InvPtr<re::Texture>& iemTexOut) const
	{
		const uint32_t iemTexWidthHeight =
			static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_iemTexWidthHeightKey));

		const re::Texture::TextureParams iemTexParams
		{
			.m_width = iemTexWidthHeight,
			.m_height = iemTexWidthHeight,
			.m_usage = static_cast<re::Texture::Usage>(re::Texture::ColorTarget | re::Texture::ColorSrc),
			.m_dimension = re::Texture::Dimension::TextureCube,
			.m_format = re::Texture::Format::RGBA16F,
			.m_colorSpace = re::Texture::ColorSpace::Linear,
			.m_mipMode = re::Texture::MipMode::None, 
		};

		std::string const& iemTexName = iblTex->GetName() + "_IEMTexture";
		iemTexOut = re::Texture::Create(iemTexName, iemTexParams);

		for (uint32_t face = 0; face < 6; face++)
		{
			re::Stage::GraphicsStageParams gfxStageParams;
			std::shared_ptr<re::Stage> iemStage = re::Stage::CreateSingleFrameGraphicsStage(
				std::format("IEM generation: Face {}/6", face + 1).c_str(), gfxStageParams);

			iemStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_IEMGeneration);
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

			pipeline->AppendSingleFrameStage(std::move(iemStage));
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

				re::Stage::GraphicsStageParams gfxStageParams;
				std::shared_ptr<re::Stage> pmremStage = re::Stage::CreateSingleFrameGraphicsStage(
					stageName.c_str(), gfxStageParams);

				pmremStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_PMREMGeneration);

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

				pipeline->AppendSingleFrameStage(std::move(pmremStage));
			}
		}
	}


	void DeferredLightingGraphicsSystem::InitializeResourceGenerationStages(
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		m_resourceCreationStagePipeline = &pipeline;

		m_resourceCreationStageParentItr = pipeline.AppendStage(
			re::Stage::CreateParentStage("Resource creation stages parent"));


		// Cube mesh, for rendering of IBL cubemaps
		if (m_cubeMeshPrimitive == nullptr)
		{
			m_cubeMeshPrimitive = gr::meshfactory::CreateCube(gr::meshfactory::FactoryOptions{
				.m_inventory = re::RenderManager::Get()->GetInventory(), });
		}

		// Create a cube mesh batch, for reuse during the initial frame IBL rendering:
		if (m_cubeMeshBatch == nullptr)
		{
			m_cubeMeshBatch = std::make_unique<re::BatchHandle>(RasterBatchBuilder::CreateMeshPrimitiveBatch(
				m_cubeMeshPrimitive, 
				k_deferredLightingEffectID, 
				grutil::BuildMeshPrimitiveRasterBatch)
					.BuildPermanent());
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

		m_lightIDToShadowRecords = GetDataDependency<LightIDToShadowRecordMap>(k_lightIDToShadowRecordInput, dataDependencies);
		m_PCSSSampleParamsBuffer = bufferDependencies.at(k_PCSSSampleParamsBufferInput);


		m_missing2DShadowFallback = re::Texture::Create("Missing 2D shadow fallback",
			re::Texture::TextureParams
			{
				.m_usage = re::Texture::Usage::ColorSrc,
				.m_dimension = re::Texture::Dimension::Texture2D,
				.m_format = re::Texture::Format::Depth32F,
				.m_colorSpace = re::Texture::ColorSpace::Linear,
				.m_mipMode = re::Texture::MipMode::None,
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
			},
			glm::vec4(1.f, 1.f, 1.f, 1.f));

		re::Stage::GraphicsStageParams gfxStageParams;
		m_ambientStage = re::Stage::CreateGraphicsStage("Ambient light stage", gfxStageParams);

		m_directionalStage = re::Stage::CreateGraphicsStage("Directional light stage", gfxStageParams);
		m_pointStage = re::Stage::CreateGraphicsStage("Point light stage", gfxStageParams);
		m_spotStage = re::Stage::CreateGraphicsStage("Spot light stage", gfxStageParams);

		m_fullscreenStage = re::Stage::CreateComputeStage("Deferred Fullscreen stage", re::Stage::ComputeStageParams{});

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

		core::InvPtr<re::Texture> const& lightTargetTex = re::Texture::Create("DeferredLightTarget", lightTargetTexParams);

		// Create the lighting target set (shared by all lights/stages):
		re::TextureTarget::TargetParams deferredTargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1)};

		m_lightingTargetSet->SetColorTarget(0, lightTargetTex, deferredTargetParams);

		// We need the depth buffer attached, but with depth writes disabled:
		re::TextureTarget::TargetParams depthTargetParams{ .m_textureView = {
				re::TextureView::Texture2DView(0, 1),
				{re::TextureView::ViewFlags::ReadOnlyDepth} } };

		m_lightingTargetSet->SetDepthStencilTarget(
			*texDependencies.at(GBufferGraphicsSystem::GBufferTexNameHashKeys[GBufferGraphicsSystem::GBufferDepth]),
			depthTargetParams);

		// Append a color-only clear stage to clear the lighting target:
		std::shared_ptr<re::ClearTargetSetStage> clearStage = 
			re::Stage::CreateTargetSetClearStage("DeferredLighting: Clear lighting targets", m_lightingTargetSet);
		clearStage->EnableAllColorClear();

		pipeline.AppendStage(clearStage);


		// Ambient stage:
		// --------------
		m_ambientStage->SetTextureTargetSet(m_lightingTargetSet);

		m_ambientStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_DeferredAmbient);

		m_ambientStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());	

		// Get/set the AO texture. If it doesn't exist, we'll get a default opaque white texture
		m_ssaoTex = *texDependencies.at(k_ssaoInput);

		core::InvPtr<re::Sampler> const& clampMinMagMipPoint = re::Sampler::GetSampler("ClampMinMagMipPoint");

		m_ambientStage->AddPermanentTextureInput(
			k_ssaoInput.GetKey(), m_ssaoTex, clampMinMagMipPoint, re::TextureView(m_ssaoTex));

		// Append the ambient stage:
		pipeline.AppendStage(m_ambientStage);
		

		// Directional light stage:
		//-------------------------
		m_directionalStage->SetTextureTargetSet(m_lightingTargetSet);
		
		m_directionalStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_DeferredDirectional);

		m_directionalStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_directionalStage->AddPermanentBuffer(PoissonSampleParamsData::s_shaderName, *m_PCSSSampleParamsBuffer);

		pipeline.AppendStage(m_directionalStage);


		// Fullscreen stage:
		//------------------
		m_fullscreenStage->AddPermanentRWTextureInput(
			"LightingTarget",
			m_lightingTargetSet->GetColorTarget(0).GetTexture(),
			re::TextureView(m_lightingTargetSet->GetColorTarget(0).GetTexture()));

		m_fullscreenStage->AddPermanentBuffer(m_lightingTargetSet->GetCreateTargetParamsBuffer());

		m_fullscreenStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_Fullscreen);

		pipeline.AppendStage(m_fullscreenStage);

		// Construct a permanent compute batch for the fullscreen stage:
		const uint32_t roundedXDim = 
			grutil::GetRoundedDispatchDimension(m_lightingTargetSet->GetViewport().Width(), k_dispatchXYThreadDims);
		const uint32_t roundedYDim = 
			grutil::GetRoundedDispatchDimension(m_lightingTargetSet->GetViewport().Height(), k_dispatchXYThreadDims);

		m_fullscreenComputeBatch = std::make_unique<re::BatchHandle>(gr::ComputeBatchBuilder()
			.SetThreadGroupCount(glm::uvec3(roundedXDim, roundedYDim, 1u))
			.SetEffectID(k_deferredLightingEffectID)
			.BuildPermanent());


		// Point light stage:
		//-------------------
		m_pointStage->SetTextureTargetSet(m_lightingTargetSet);
		m_pointStage->AddPermanentBuffer(m_lightingTargetSet->GetCreateTargetParamsBuffer());

		m_pointStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_pointStage->AddPermanentBuffer(PoissonSampleParamsData::s_shaderName, *m_PCSSSampleParamsBuffer);

		m_pointStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_DeferredPoint);

		pipeline.AppendStage(m_pointStage);


		// Spot light stage:
		//------------------
		m_spotStage->SetTextureTargetSet(m_lightingTargetSet);
		m_spotStage->AddPermanentBuffer(m_lightingTargetSet->GetCreateTargetParamsBuffer());

		m_spotStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_spotStage->AddPermanentBuffer(PoissonSampleParamsData::s_shaderName, *m_PCSSSampleParamsBuffer);

		m_spotStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_DeferredSpot);

		pipeline.AppendStage(m_spotStage);


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

			util::CHashKey const& texName = GBufferGraphicsSystem::GBufferTexNameHashKeys[slot];
			core::InvPtr<re::Texture> const& gbufferTex = *texDependencies.at(texName);
			
			m_ambientStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, re::TextureView(gbufferTex));
			m_directionalStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, re::TextureView(gbufferTex));

			m_fullscreenStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, re::TextureView(gbufferTex));

			m_pointStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, re::TextureView(gbufferTex));
			m_spotStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, re::TextureView(gbufferTex));
		}


		// Attach the GBUffer depth input:
		constexpr uint8_t depthBufferSlot = gr::GBufferGraphicsSystem::GBufferDepth;
		util::CHashKey const& depthName = GBufferGraphicsSystem::GBufferTexNameHashKeys[depthBufferSlot];
		core::InvPtr<re::Texture> const& depthTex = *texDependencies.at(depthName);

		m_ambientStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, re::TextureView(depthTex));
		m_directionalStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, re::TextureView(depthTex));

		m_fullscreenStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, re::TextureView(depthTex));

		m_pointStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, re::TextureView(depthTex));
		m_spotStage->AddPermanentTextureInput(
			depthName.GetKey(), depthTex, wrapMinMagLinearMipPoint, re::TextureView(depthTex));
		
		m_ambientStage->AddPermanentTextureInput(
			"DFG", m_BRDF_integrationMap, clampMinMagMipPoint, re::TextureView(m_BRDF_integrationMap));
	}


	void DeferredLightingGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		gr::IndexedBufferManager& ibm = renderData.GetInstancingIndexedBufferManager();

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
		if (deletedAmbientIDs)
		{
			for (auto const& deletedAmbientID : *deletedAmbientIDs)
			{
				if (deletedAmbientID == m_activeAmbientLightData.m_renderDataID)
				{
					m_activeAmbientLightData = {};
					break;
				}
			}
			DeleteLights(deletedAmbientIDs, m_ambientLightData);
		}
		
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
				for (auto const& ambientItr : gr::IDAdapter(renderData, *newAmbientIDs))
				{
					gr::Light::RenderDataAmbientIBL const& ambientData = ambientItr->Get<gr::Light::RenderDataAmbientIBL>();

					const gr::RenderDataID lightID = ambientData.m_renderDataID;

					core::InvPtr<re::Texture> const& iblTex = ambientData.m_iblTex;
					SEAssert(iblTex, "IBL texture cannot be null");

					core::InvPtr<re::Texture> iemTex;
					PopulateIEMTex(m_resourceCreationStagePipeline, iblTex, iemTex);

					core::InvPtr<re::Texture> pmremTex;
					PopulatePMREMTex(m_resourceCreationStagePipeline, iblTex, pmremTex);

					const uint32_t totalPMREMMipLevels = pmremTex->GetNumMips();

					const AmbientLightData ambientLightParamsData = GetAmbientLightData(
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
							.m_batch = gr::RasterBatchBuilder::CreateInstance(
								lightID, renderData, grutil::BuildInstancedRasterBatch)
									.SetEffectID(k_deferredLightingEffectID)
									.SetTextureInput(
										"CubeMapIEM",
										iemTex,
										re::Sampler::GetSampler("WrapMinMagMipLinear"),
										re::TextureView(iemTex))
									.SetTextureInput(
										"CubeMapPMREM",
										pmremTex,
										re::Sampler::GetSampler("WrapMinMagMipLinear"),
										re::TextureView(pmremTex))
									.SetBuffer(AmbientLightData::s_shaderName, ambientParams)
									.BuildPermanent()
						});
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

				const AmbientLightData ambientLightParamsData = GetAmbientLightData(
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
				for (auto const& directionalItr : gr::IDAdapter(renderData, *newDirectionalIDs))
				{
					gr::Light::RenderDataDirectional const& directionalData =
						directionalItr->Get<gr::Light::RenderDataDirectional>();

					const gr::RenderDataID lightID = directionalItr->GetRenderDataID();

					m_punctualLightData.emplace(
						lightID,
						PunctualLightRenderData{
							.m_type = gr::Light::Directional,
							.m_batch = gr::RasterBatchBuilder::CreateInstance(
								lightID, renderData, grutil::BuildInstancedRasterBatch)
									.SetEffectID(k_deferredLightingEffectID)
									.BuildPermanent(),
							.m_hasShadow = directionalData.m_hasShadow
						});
					
					// Note: We set the shadow texture inputs per frame/batch if/as required
				}
			}
		}


		auto RegisterNewDeferredMeshLight = [&](
			auto const& lightItr,
			gr::Light::Type lightType,
			void const* lightRenderData,
			bool hasShadow,
			std::unordered_map<gr::RenderDataID, PunctualLightRenderData>& punctualLightData)
			{
				const gr::RenderDataID lightID = lightItr->GetRenderDataID();

				punctualLightData.emplace(
					lightID,
					PunctualLightRenderData{
						.m_type = lightType,
						.m_batch = gr::RasterBatchBuilder::CreateInstance(
							lightID, renderData, grutil::BuildInstancedRasterBatch)
								.SetEffectID(k_deferredLightingEffectID)
								.BuildPermanent(),
						.m_hasShadow = hasShadow
					});
				
				// Note: We set the shadow texture inputs per frame/batch if/as required
			};
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataPoint>())
		{
			std::vector<gr::RenderDataID> const* newPointIDs = renderData.GetIDsWithNewData<gr::Light::RenderDataPoint>();
			if (newPointIDs)
			{
				for (auto const& pointItr : gr::IDAdapter(renderData, *newPointIDs))
				{
					gr::Light::RenderDataPoint const& pointData = pointItr->Get<gr::Light::RenderDataPoint>();
					const bool hasShadow = pointData.m_hasShadow;

					RegisterNewDeferredMeshLight(pointItr, gr::Light::Point, &pointData, hasShadow, m_punctualLightData);
				}
			}
		}
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataSpot>())
		{
			std::vector<gr::RenderDataID> const* newSpotIDs = renderData.GetIDsWithNewData<gr::Light::RenderDataSpot>();
			if (newSpotIDs)
			{
				for (auto const& spotItr : gr::IDAdapter(renderData, *newSpotIDs))
				{
					gr::Light::RenderDataSpot const& spotData = spotItr->Get<gr::Light::RenderDataSpot>();
					const bool hasShadow = spotData.m_hasShadow;

					RegisterNewDeferredMeshLight(spotItr, gr::Light::Spot, &spotData, hasShadow, m_punctualLightData);
				}
			}
		}


		// Attach the indexed monolithic light data buffers:
		m_directionalStage->AddSingleFrameBuffer(ibm.GetIndexedBufferInput(
			LightData::s_directionalLightDataShaderName, LightData::s_directionalLightDataShaderName));

		m_pointStage->AddSingleFrameBuffer(ibm.GetIndexedBufferInput(
			LightData::s_pointLightDataShaderName, LightData::s_pointLightDataShaderName));

		m_spotStage->AddSingleFrameBuffer(ibm.GetIndexedBufferInput(
			LightData::s_spotLightDataShaderName, LightData::s_spotLightDataShaderName));


		// Attach the indexed monolithic shadow data buffers:
		m_directionalStage->AddSingleFrameBuffer(
			ibm.GetIndexedBufferInput(ShadowData::s_shaderName, ShadowData::s_shaderName));

		m_pointStage->AddSingleFrameBuffer(
			ibm.GetIndexedBufferInput(ShadowData::s_shaderName, ShadowData::s_shaderName));

		m_spotStage->AddSingleFrameBuffer(
			ibm.GetIndexedBufferInput(ShadowData::s_shaderName, ShadowData::s_shaderName));


		CreateBatches();
	}


	void DeferredLightingGraphicsSystem::CreateBatches()
	{
		// TODO: Instance deferred mesh lights draws via a single batch
		// TODO: Convert fullscreen lights into a single compute shader dispatch
	
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		gr::IndexedBufferManager& ibm = renderData.GetInstancingIndexedBufferManager();

		if (m_activeAmbientLightData.m_renderDataID != gr::k_invalidRenderDataID)
		{
			SEAssert(m_ambientLightData.contains(m_activeAmbientLightData.m_renderDataID),
				"Cannot find active ambient light");

			m_ambientStage->AddBatch(m_ambientLightData.at(m_activeAmbientLightData.m_renderDataID).m_batch);
		}

		m_fullscreenStage->AddBatch(*m_fullscreenComputeBatch);
		
		// Hash culled visible light IDs so we can quickly check if we need to add a point/spot light's batch:
		std::unordered_set<gr::RenderDataID> visibleLightIDs;

		auto MarkIDsVisible = [&](std::vector<gr::RenderDataID> const* lightIDs)
			{
				for (gr::RenderDataID lightID : *lightIDs)
				{
					visibleLightIDs.emplace(lightID);
				}
			};
		auto MarkAllIDsVisible = [&](auto&& lightObjectItr)
			{
				for (auto const& itr : lightObjectItr)
				{
					visibleLightIDs.emplace(itr->GetRenderDataID());
				}
			};

		if (m_spotCullingResults)
		{
			MarkIDsVisible(m_spotCullingResults);
		}
		else
		{
			MarkAllIDsVisible(gr::IDAdapter(renderData, *renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataSpot>()));			
		}

		if (m_pointCullingResults)
		{
			MarkIDsVisible(m_pointCullingResults);
		}
		else
		{
			MarkAllIDsVisible(gr::IDAdapter(renderData, *renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataPoint>()));
		}


		// Update all of the punctual lights we're tracking:
		for (auto& light : m_punctualLightData)
		{
			const gr::RenderDataID lightID = light.first;

			// Update lighting buffers, if anything is dirty:
			const bool lightRenderDataDirty = 
				(light.second.m_type == gr::Light::Type::Directional &&
					renderData.IsDirty<gr::Light::RenderDataDirectional>(lightID)) ||
				(light.second.m_type == gr::Light::Type::Point &&
					renderData.IsDirty<gr::Light::RenderDataPoint>(lightID)) ||
				(light.second.m_type == gr::Light::Type::Spot &&
					renderData.IsDirty<gr::Light::RenderDataSpot>(lightID));

			if (lightRenderDataDirty)
			{
				switch (light.second.m_type)
				{
				case gr::Light::Type::Directional:
				{
					gr::Light::RenderDataDirectional const& directionalData =
						renderData.GetObjectData<gr::Light::RenderDataDirectional>(lightID);
					light.second.m_canContribute = directionalData.m_canContribute;
				}
				break;
				case gr::Light::Type::Point:
				{
					gr::Light::RenderDataPoint const& pointData = 
						renderData.GetObjectData<gr::Light::RenderDataPoint>(lightID);
					light.second.m_canContribute = pointData.m_canContribute;
				}
				break;
				case gr::Light::Type::Spot:
				{
					gr::Light::RenderDataSpot const& spotData =
						renderData.GetObjectData<gr::Light::RenderDataSpot>(lightID);
					light.second.m_canContribute = spotData.m_canContribute;
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
				auto AddDuplicatedBatch = [&light, &lightID, &ibm, this](
					re::Stage* stage,
					char const* shadowTexShaderName,
					util::HashKey const& samplerTypeName)
					{
						re::BatchHandle& duplicatedBatch =
							*stage->AddBatchWithLifetime(light.second.m_batch, re::Lifetime::SingleFrame);

						uint32_t shadowTexArrayIdx = INVALID_SHADOW_IDX;
						if (light.second.m_hasShadow)
						{
							SEAssert(m_lightIDToShadowRecords->contains(lightID), "Failed to find a shadow record");
							gr::ShadowRecord const& shadowRecord = m_lightIDToShadowRecords->at(lightID);

							shadowTexArrayIdx = shadowRecord.m_shadowTexArrayIdx;

							// Note: Shadow array textures may be reallocated at the beginning of any frame; Texture
							// inputs/views must be re-set each frame (TODO: Skip recreating the views by tracking 
							// texture changes)						
							duplicatedBatch->SetTextureInput(
								shadowTexShaderName,
								*shadowRecord.m_shadowTex,
								re::Sampler::GetSampler(samplerTypeName),
								CreateShadowArrayReadView(*shadowRecord.m_shadowTex));
						}

						char const* shaderName = nullptr;
						switch (light.second.m_type)
						{
						case gr::Light::Type::Directional:
						{
							shaderName = LightShadowLUTData::s_shaderNameDirectional;
						}
						break;
						case gr::Light::Type::Point:
						{
							shaderName = LightShadowLUTData::s_shaderNamePoint;

							// Add the Transform and instanced index LUT:
							duplicatedBatch->SetBuffer(
								ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));

							duplicatedBatch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
								InstanceIndexData::s_shaderName, std::views::single(lightID)));
						}
						break;
						case gr::Light::Type::Spot:
						{
							shaderName = LightShadowLUTData::s_shaderNameSpot;

							// Add the Transform and instanced index LUT:
							duplicatedBatch->SetBuffer(
								ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));

							duplicatedBatch->SetBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
								InstanceIndexData::s_shaderName, std::views::single(lightID)));
						}
						break;						
						case gr::Light::Type::AmbientIBL:
						default: SEAssertF("Invalid light type");
						}

						// Pre-populate and add our light data LUT buffer:
						const LightShadowLUTData lightShadowLUT{
							.g_lightShadowIdx = glm::uvec4(
								0,					// Light buffer idx
								INVALID_SHADOW_IDX, // Shadow buffer idx: Will be overwritten IFF a shadow exists
								shadowTexArrayIdx,
								light.second.m_type),
						};

						duplicatedBatch->SetBuffer(ibm.GetLUTBufferInput<LightShadowLUTData>(
							shaderName,
							{ lightShadowLUT },
							std::span<const gr::RenderDataID>{&lightID, 1}));
					};

				const util::HashKey sampler2DShadowName("BorderCmpMinMagLinearMipPoint");
				const util::HashKey samplerCubeShadowName("WrapCmpMinMagLinearMipPoint");
				switch (light.second.m_type)
				{
				case gr::Light::Type::Directional:
				{
					AddDuplicatedBatch(m_directionalStage.get(), "DirectionalShadows", sampler2DShadowName);
				}
				break;
				case gr::Light::Type::Point:
				{
					AddDuplicatedBatch(m_pointStage.get(), "PointShadows", samplerCubeShadowName);
				}
				break;
				case gr::Light::Type::Spot:
				{
					AddDuplicatedBatch(m_spotStage.get(), "SpotShadows", sampler2DShadowName);
				}
				break;
				case gr::Light::Type::AmbientIBL:
				default: SEAssertF("Invalid light type");
				}
			}
		}
	}
}