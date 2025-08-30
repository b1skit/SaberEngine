// © 2025 Adam Badke. All rights reserved.
#include "BatchBuilder.h"
#include "BatchFactories.h"
#include "Buffer.h"
#include "Effect.h"
#include "GraphicsEvent.h"
#include "GraphicsSystem_DeferredIBL.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "GraphicsUtils.h"
#include "IndexedBuffer.h"
#include "LightParamsHelpers.h"
#include "LightRenderData.h"
#include "MeshFactory.h"
#include "RenderDataManager.h"
#include "RenderObjectIDs.h"
#include "Sampler.h"
#include "Stage.h"
#include "TextureTarget.h"
#include "TextureView.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Util/CHashKey.h"

#include "Renderer/Shaders/Common/IBLGenerationParams.h"
#include "Renderer/Shaders/Common/LightParams.h"


namespace
{
	static const EffectID k_deferredLightingEffectID = effect::Effect::ComputeEffectID("DeferredLighting");


	BRDFIntegrationData GetBRDFIntegrationParamsDataData()
	{
		const uint32_t brdfTexWidthHeight =
			static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey));

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

		const int numIEMSamples = core::Config::GetValue<int>(core::configkeys::k_iemNumSamplesKey);
		const int numPMREMSamples = core::Config::GetValue<int>(core::configkeys::k_pmremNumSamplesKey);

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
			glm::log2(glm::sqrt(maxDimension)) + 1,
			srcWidth,
			srcHeight,
			numMipLevels);

		return generationParams;
	}
}


namespace gr
{
	DeferredIBLGraphicsSystem::DeferredIBLGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_resourceCreationStagePipeline(nullptr)
	{
		m_lightingTargetSet = re::TextureTargetSet::Create("Deferred light targets");
	}


	void DeferredIBLGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_lightingTargetTexInput);
		RegisterTextureInput(k_AOTexInput, TextureInputDefault::OpaqueWhite);

		// Deferred lighting GS is (currently) tightly coupled to the GBuffer GS
		for (uint8_t slot = 0; slot < GBufferGraphicsSystem::GBufferTexIdx_Count; slot++)
		{
			if (slot == GBufferGraphicsSystem::GBufferEmissive)
			{
				continue;
			}

			RegisterTextureInput(GBufferGraphicsSystem::GBufferTexNameHashKeys[slot]);
		}
	}


	void DeferredIBLGraphicsSystem::RegisterOutputs()
	{
		RegisterTextureOutput(k_activeAmbientIEMTexOutput, &m_activeAmbientLightData.m_IEMTex);
		RegisterTextureOutput(k_activeAmbientPMREMTexOutput, &m_activeAmbientLightData.m_PMREMTex);
		RegisterTextureOutput(k_activeAmbientDFGTexOutput, &m_BRDF_integrationMap);

		RegisterBufferOutput(k_activeAmbientParamsBufferOutput, &m_activeAmbientLightData.m_ambientParams);
	}


	void DeferredIBLGraphicsSystem::CreateSingleFrameBRDFPreIntegrationStage(gr::StagePipeline& pipeline)
	{
		gr::Stage::ComputeStageParams computeStageParams;
		std::shared_ptr<gr::Stage> brdfStage =
			gr::Stage::CreateSingleFrameComputeStage("BRDF pre-integration compute stage", computeStageParams);

		brdfStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_BRDFIntegration);

		const uint32_t brdfTexWidthHeight =
			static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey));

		// Create a render target texture:			
		re::Texture::TextureParams brdfParams;
		brdfParams.m_width = brdfTexWidthHeight;
		brdfParams.m_height = brdfTexWidthHeight;
		brdfParams.m_usage = re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc;
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
			.Build());

		pipeline.AppendSingleFrameStage(std::move(brdfStage));
	}


	void DeferredIBLGraphicsSystem::PopulateIEMTex(
		gr::StagePipeline* pipeline, core::InvPtr<re::Texture> const& iblTex, core::InvPtr<re::Texture>& iemTexOut) const
	{
		const uint32_t iemTexWidthHeight =
			static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_iemTexWidthHeightKey));

		const re::Texture::TextureParams iemTexParams
		{
			.m_width = iemTexWidthHeight,
			.m_height = iemTexWidthHeight,
			.m_usage = re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc,
			.m_dimension = re::Texture::Dimension::TextureCube,
			.m_format = re::Texture::Format::RGBA16F,
			.m_colorSpace = re::Texture::ColorSpace::Linear,
			.m_mipMode = re::Texture::MipMode::None,
		};

		std::string const& iemTexName = iblTex->GetName() + "_IEMTexture";
		iemTexOut = re::Texture::Create(iemTexName, iemTexParams);

		for (uint32_t face = 0; face < 6; face++)
		{
			gr::Stage::GraphicsStageParams gfxStageParams;
			std::shared_ptr<gr::Stage> iemStage = gr::Stage::CreateSingleFrameGraphicsStage(
				std::format("IEM generation: Face {}/6", face + 1).c_str(), gfxStageParams);

			iemStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_IEMGeneration);
			iemStage->AddPermanentTextureInput(
				"Tex0",
				iblTex,
				m_graphicsSystemManager->GetSampler("WrapMinMagLinearMipPoint"),
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
				re::TextureTarget::TargetParams{ .m_textureView = re::TextureView::Texture2DArrayView(0, 1, face, 1) });
			iemTargets->SetViewport(re::Viewport(0, 0, iemTexWidthHeight, iemTexWidthHeight));
			iemTargets->SetScissorRect(re::ScissorRect(0, 0, iemTexWidthHeight, iemTexWidthHeight));

			iemStage->SetTextureTargetSet(iemTargets);

			iemStage->AddBatch(m_cubeMeshBatch);

			pipeline->AppendSingleFrameStage(std::move(iemStage));
		}
	}


	void DeferredIBLGraphicsSystem::PopulatePMREMTex(
		gr::StagePipeline* pipeline, core::InvPtr<re::Texture> const& iblTex, core::InvPtr<re::Texture>& pmremTexOut) const
	{
		const uint32_t pmremTexWidthHeight =
			static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_pmremTexWidthHeightKey));

		// PMREM-specific texture params:
		re::Texture::TextureParams pmremTexParams;
		pmremTexParams.m_width = pmremTexWidthHeight;
		pmremTexParams.m_height = pmremTexWidthHeight;
		pmremTexParams.m_usage = re::Texture::Usage::ColorTarget | re::Texture::Usage::ColorSrc;
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

				gr::Stage::GraphicsStageParams gfxStageParams;
				std::shared_ptr<gr::Stage> pmremStage = gr::Stage::CreateSingleFrameGraphicsStage(
					stageName.c_str(), gfxStageParams);

				pmremStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_PMREMGeneration);

				pmremStage->AddPermanentTextureInput(
					"Tex0",
					iblTex,
					m_graphicsSystemManager->GetSampler("ClampMinMagMipLinear"),
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

				pmremStage->AddBatch(m_cubeMeshBatch);

				pipeline->AppendSingleFrameStage(std::move(pmremStage));
			}
		}
	}


	void DeferredIBLGraphicsSystem::InitializeResourceGenerationStages(
		gr::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		m_resourceCreationStagePipeline = &pipeline;

		m_resourceCreationStageParentItr = pipeline.AppendStage(
			gr::Stage::CreateParentStage("Resource creation stages parent"));


		// Cube mesh, for rendering of IBL cubemaps
		if (m_cubeMeshPrimitive == nullptr)
		{
			m_cubeMeshPrimitive = gr::meshfactory::CreateCube(gr::meshfactory::FactoryOptions{});
		}

		// Create a cube mesh batch, for reuse during the initial frame IBL rendering:
		if (m_cubeMeshBatch.IsValid() == false)
		{
			m_cubeMeshBatch = RasterBatchBuilder::CreateMeshPrimitiveBatch(
				m_cubeMeshPrimitive,
				k_deferredLightingEffectID,
				grutil::BuildMeshPrimitiveRasterBatch)
				.Build();
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


	void DeferredIBLGraphicsSystem::InitPipeline(
		gr::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const& bufferDependencies,
		DataDependencies const& dataDependencies)
	{
		m_ambientStage = gr::Stage::CreateGraphicsStage("Ambient light stage", {});

		// Create the lighting target set:
		m_lightingTargetSet->SetColorTarget(
			0,
			*GetDependency<core::InvPtr<re::Texture>>(k_lightingTargetTexInput, texDependencies),
			re::TextureTarget::TargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1) });

		// We need the depth buffer attached, but with depth writes disabled:
		m_lightingTargetSet->SetDepthStencilTarget(
			*GetDependency<core::InvPtr<re::Texture>>(
				GBufferGraphicsSystem::GBufferTexNameHashKeys[GBufferGraphicsSystem::GBufferDepth], texDependencies),
			re::TextureTarget::TargetParams{ .m_textureView = {
				re::TextureView::Texture2DView(0, 1),
				{re::TextureView::ViewFlags::ReadOnlyDepth} } });


		// Ambient stage:
		// --------------
		m_ambientStage->SetTextureTargetSet(m_lightingTargetSet);

		m_ambientStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_DeferredAmbient);

		m_ambientStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		// Get/set the AO texture. If it doesn't exist, we'll get a default opaque white texture
		m_AOTex = *GetDependency<core::InvPtr<re::Texture>>(k_AOTexInput, texDependencies);
		
		core::InvPtr<re::Sampler> const& clampMinMagMipPoint = m_graphicsSystemManager->GetSampler("ClampMinMagMipPoint");

		m_ambientStage->AddPermanentTextureInput(
			k_AOTexInput.GetKey(),
			m_AOTex,
			clampMinMagMipPoint,
			re::TextureView(m_AOTex, re::TextureView::ViewFlags{ .m_formatOverride = re::Texture::Format::R8_UNORM, })
		);

		// Attach GBuffer inputs:
		core::InvPtr<re::Sampler> const& wrapMinMagLinearMipPoint =
			m_graphicsSystemManager->GetSampler("WrapMinMagLinearMipPoint");

		for (uint8_t slot = 0; slot < GBufferGraphicsSystem::GBufferTexIdx::GBufferTexIdx_Count; slot++)
		{
			if (slot == GBufferGraphicsSystem::GBufferEmissive)
			{
				continue; // The emissive texture is not used
			}

			SEAssert(texDependencies.contains(GBufferGraphicsSystem::GBufferTexNameHashKeys[slot]),
				"Texture dependency not found");

			util::CHashKey const& texName = GBufferGraphicsSystem::GBufferTexNameHashKeys[slot];
			core::InvPtr<re::Texture> const& gbufferTex =
				*GetDependency<core::InvPtr<re::Texture>>(texName, texDependencies);
			
			m_ambientStage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, re::TextureView(gbufferTex));
		}

		m_ambientStage->AddPermanentTextureInput(
			"DFG", m_BRDF_integrationMap, clampMinMagMipPoint, re::TextureView(m_BRDF_integrationMap));

		// Append the ambient stage:
		pipeline.AppendStage(m_ambientStage);

		// Register for events:
		m_graphicsSystemManager->SubscribeToGraphicsEvent<DeferredIBLGraphicsSystem>(
			greventkey::k_activeAmbientLightHasChanged, this);
	}


	void DeferredIBLGraphicsSystem::HandleEvents()
	{
		while (HasEvents())
		{
			gr::GraphicsEvent const& event = GetEvent();
			switch (event.m_eventKey)
			{
			case greventkey::k_activeAmbientLightHasChanged:
			{
				const gr::RenderDataID newAmbientLightID = std::get<gr::RenderDataID>(event.m_data);

				// Update the shared active ambient light pointers:
				if (newAmbientLightID != m_activeAmbientLightData.m_renderDataID)
				{
					SEAssert(m_ambientLightData.contains(newAmbientLightID), "Cannot find active ambient light");

					AmbientLightRenderData const& activeAmbientLightData = m_ambientLightData.at(newAmbientLightID);

					m_activeAmbientLightData.m_renderDataID = newAmbientLightID;
					m_activeAmbientLightData.m_ambientParams = activeAmbientLightData.m_ambientParams;
					m_activeAmbientLightData.m_IEMTex = activeAmbientLightData.m_IEMTex;
					m_activeAmbientLightData.m_PMREMTex = activeAmbientLightData.m_PMREMTex;
				}
			}
			break;
			default: SEAssertF("Unexpected event in DeferredIBLGraphicsSystem");
			}
		}
	}


	void DeferredIBLGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		gr::IndexedBufferManager& ibm = renderData.GetInstancingIndexedBufferManager();

		// Removed any deleted ambient lights, and null out the active ambient light tracking if necessary:
		std::vector<gr::RenderDataID> const* deletedAmbientIDs =
			renderData.GetIDsWithDeletedData<gr::Light::RenderDataIBL>();
		if (deletedAmbientIDs)
		{
			for (auto const& deletedAmbientID : *deletedAmbientIDs)
			{
				if (deletedAmbientID == m_activeAmbientLightData.m_renderDataID)
				{
					m_activeAmbientLightData = {};					
				}
				m_ambientLightData.erase(deletedAmbientID);
			}
		}


		// Register new ambient lights:
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataIBL>())
		{
			std::vector<gr::RenderDataID> const* newAmbientIDs =
				renderData.GetIDsWithNewData<gr::Light::RenderDataIBL>();

			if (newAmbientIDs)
			{
				for (auto const& ambientItr : gr::IDAdapter(renderData, *newAmbientIDs))
				{
					gr::Light::RenderDataIBL const& ambientData = ambientItr->Get<gr::Light::RenderDataIBL>();

					const gr::RenderDataID lightID = ambientData.m_renderDataID;

					core::InvPtr<re::Texture> const& iblTex = ambientData.m_iblTex;
					SEAssert(iblTex, "IBL texture cannot be null");

					core::InvPtr<re::Texture> iemTex;
					PopulateIEMTex(m_resourceCreationStagePipeline, iblTex, iemTex);

					core::InvPtr<re::Texture> pmremTex;
					PopulatePMREMTex(m_resourceCreationStagePipeline, iblTex, pmremTex);

					const uint32_t totalPMREMMipLevels = pmremTex->GetNumMips();

					const AmbientLightData ambientLightParamsData = grutil::GetAmbientLightData(
						totalPMREMMipLevels,
						ambientData.m_diffuseScale,
						ambientData.m_specularScale,
						static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey)),
						m_AOTex);

					std::shared_ptr<re::Buffer> const& ambientParams = re::Buffer::Create(
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
										m_graphicsSystemManager->GetSampler("WrapMinMagMipLinear"),
										re::TextureView(iemTex))
									.SetTextureInput(
										"CubeMapPMREM",
										pmremTex,
										m_graphicsSystemManager->GetSampler("WrapMinMagMipLinear"),
										re::TextureView(pmremTex))
									.SetBuffer(AmbientLightData::s_shaderName, ambientParams)
									.Build()
						});
				}
			}
		}

		// Update the params of the ambient lights we're tracking:
		for (auto& ambientLight : m_ambientLightData)
		{
			const gr::RenderDataID lightID = ambientLight.first;

			if (renderData.IsDirty<gr::Light::RenderDataIBL>(lightID))
			{
				gr::Light::RenderDataIBL const& ambientRenderData =
					renderData.GetObjectData<gr::Light::RenderDataIBL>(lightID);

				const uint32_t totalPMREMMipLevels = ambientLight.second.m_PMREMTex->GetNumMips();

				const AmbientLightData ambientLightParamsData = grutil::GetAmbientLightData(
					totalPMREMMipLevels,
					ambientRenderData.m_diffuseScale,
					ambientRenderData.m_specularScale,
					static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey)),
					m_AOTex);

				ambientLight.second.m_ambientParams->Commit(ambientLightParamsData);
			}
		}

		// Now that our ambient light tracking is updated, we can handle events:
		HandleEvents();

		CreateBatches();
	}


	void DeferredIBLGraphicsSystem::CreateBatches()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		gr::IndexedBufferManager& ibm = renderData.GetInstancingIndexedBufferManager();

		if (m_activeAmbientLightData.m_renderDataID != gr::k_invalidRenderDataID)
		{
			SEAssert(m_ambientLightData.contains(m_activeAmbientLightData.m_renderDataID),
				"Cannot find active ambient light");

			m_ambientStage->AddBatch(m_ambientLightData.at(m_activeAmbientLightData.m_renderDataID).m_batch);
		}
	}
}