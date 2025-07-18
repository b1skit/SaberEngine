// © 2024 Adam Badke. All rights reserved.
#include "IndexedBuffer.h"
#include "GraphicsSystem_Transparency.h"
#include "GraphicsSystemManager.h"
#include "LightParamsHelpers.h"
#include "Sampler.h"

#include "Core/Config.h"

#include "Renderer/Shaders/Common/ShadowParams.h"


namespace
{
	void CreateUpdateLightMetadata(
		LightMetadata& lightMetadata,
		re::BufferInput& lightMetadataBufferInput,
		gr::RenderDataManager const& renderData,
		gr::PunctualLightCullingResults const* pointCullingIDs,
		gr::PunctualLightCullingResults const* spotCullingIDs,
		char const* bufferName)
	{
		const uint32_t numDirectional = renderData.GetNumElementsOfType<gr::Light::RenderDataDirectional>();
		const uint32_t numPoint = pointCullingIDs ? util::CheckedCast<uint32_t>(pointCullingIDs->size()) : 0;
		const uint32_t numSpot = spotCullingIDs ? util::CheckedCast<uint32_t>(spotCullingIDs->size()) : 0;

		if (lightMetadataBufferInput.GetBuffer() != nullptr &&
			lightMetadata.g_numLights.x == numDirectional &&
			lightMetadata.g_numLights.y == numPoint &&
			lightMetadata.g_numLights.z == numSpot)
		{
			return; // Nothing to update
		}

		lightMetadata.g_numLights = glm::uvec4(numDirectional, numPoint, numSpot, 0);

		if (lightMetadataBufferInput.GetBuffer() == nullptr)
		{
			lightMetadataBufferInput = re::BufferInput(
				bufferName,
				re::Buffer::Create(
					bufferName,
					lightMetadata,
					re::Buffer::BufferParams{
						.m_lifetime = re::Lifetime::Permanent,
						.m_stagingPool = re::Buffer::StagingPool::Permanent,
						.m_memPoolPreference = re::Buffer::UploadHeap,
						.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
						.m_usageMask = re::Buffer::Constant,
					}));
		}
		else
		{
			lightMetadataBufferInput.GetBuffer()->Commit(lightMetadata);
		}
	}
}

namespace gr
{
	TransparencyGraphicsSystem::TransparencyGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_ambientIEMTex(nullptr)
		, m_ambientPMREMTex(nullptr)
		, m_pointCullingResults(nullptr)
		, m_spotCullingResults(nullptr)
		, m_PCSSSampleParamsBuffer(nullptr)
	{
	}


	void TransparencyGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_sceneDepthTexInput);
		RegisterTextureInput(k_sceneLightingTexInput);
		RegisterTextureInput(k_ambientIEMTexInput, TextureInputDefault::CubeMap_OpaqueBlack);
		RegisterTextureInput(k_ambientPMREMTexInput, TextureInputDefault::CubeMap_OpaqueBlack);
		RegisterTextureInput(k_ambientDFGTexInput);

		RegisterBufferInput(k_ambientParamsBufferInput);

		RegisterDataInput(k_pointLightCullingDataInput);
		RegisterDataInput(k_spotLightCullingDataInput);

		RegisterDataInput(k_viewBatchesDataInput);
		RegisterDataInput(k_allBatchesDataInput);

		RegisterTextureInput(k_directionalShadowArrayTexInput);
		RegisterTextureInput(k_pointShadowArrayTexInput);
		RegisterTextureInput(k_spotShadowArrayTexInput);
		
		RegisterDataInput(k_lightIDToShadowRecordInput);

		RegisterBufferInput(k_PCSSSampleParamsBufferInput);
	}


	void TransparencyGraphicsSystem::RegisterOutputs()
	{
		//
	}


	void TransparencyGraphicsSystem::InitPipeline(
		gr::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const& bufferDependencies,
		DataDependencies const& dataDependencies)
	{
		SEAssert(texDependencies.contains(k_ambientIEMTexInput) &&
			texDependencies.contains(k_ambientPMREMTexInput) &&
			texDependencies.contains(k_sceneLightingTexInput) &&
			texDependencies.contains(k_ambientDFGTexInput) &&
			bufferDependencies.contains(k_ambientParamsBufferInput) &&
			bufferDependencies.at(k_ambientParamsBufferInput) != nullptr,
			"Missing a required input");

		// Cache our dependencies:
		m_ambientIEMTex = texDependencies.at(k_ambientIEMTexInput);
		m_ambientPMREMTex = texDependencies.at(k_ambientPMREMTexInput);
		m_ambientParams = bufferDependencies.at(k_ambientParamsBufferInput);

		m_pointCullingResults = GetDataDependency<PunctualLightCullingResults>(k_pointLightCullingDataInput, dataDependencies);
		m_spotCullingResults = GetDataDependency<PunctualLightCullingResults>(k_spotLightCullingDataInput, dataDependencies);

		m_viewBatches = GetDataDependency<ViewBatches>(k_viewBatchesDataInput, dataDependencies);
		m_allBatches = GetDataDependency<AllBatches>(k_allBatchesDataInput, dataDependencies);
		SEAssert(m_viewBatches || m_allBatches, "Must have received some batches");

		m_directionalShadowArrayTex = texDependencies.at(k_directionalShadowArrayTexInput);
		m_pointShadowArrayTex = texDependencies.at(k_pointShadowArrayTexInput);
		m_spotShadowArrayTex = texDependencies.at(k_spotShadowArrayTexInput);
		
		m_lightIDToShadowRecords = GetDataDependency<LightIDToShadowRecordMap>(k_lightIDToShadowRecordInput, dataDependencies);

		m_PCSSSampleParamsBuffer = bufferDependencies.at(k_PCSSSampleParamsBufferInput);


		// Stage setup:
		m_transparencyStage = gr::Stage::CreateGraphicsStage("Transparency Stage", {});

		m_transparencyStage->SetBatchFilterMaskBit(
			gr::Batch::Filter::AlphaBlended, gr::Stage::FilterMode::Require, true);

		m_transparencyStage->AddDrawStyleBits(effect::drawstyle::RenderPath_Forward);

		// Targets:
		std::shared_ptr<re::TextureTargetSet> transparencyTarget = re::TextureTargetSet::Create("Transparency Targets");

		transparencyTarget->SetColorTarget(0,
			*texDependencies.at(k_sceneLightingTexInput),
			re::TextureTarget::TargetParams{ .m_textureView = {re::TextureView::Texture2DView(0, 1) } });

		transparencyTarget->SetDepthStencilTarget(
			*texDependencies.at(k_sceneDepthTexInput),
			re::TextureTarget::TargetParams{ .m_textureView = {
				re::TextureView::Texture2DView(0, 1),
				{re::TextureView::ViewFlags::ReadOnlyDepth} } });

		m_transparencyStage->SetTextureTargetSet(transparencyTarget);

		// Buffers:		
		m_transparencyStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_transparencyStage->AddPermanentBuffer(transparencyTarget->GetCreateTargetParamsBuffer());
		m_transparencyStage->AddPermanentBuffer(PoissonSampleParamsData::s_shaderName, *m_PCSSSampleParamsBuffer);

		// Texture inputs:
		m_transparencyStage->AddPermanentTextureInput(
			"DFG",
			*texDependencies.at(k_ambientDFGTexInput),
			m_graphicsSystemManager->GetSampler("ClampMinMagMipPoint"),
			re::TextureView(*texDependencies.at(k_ambientDFGTexInput)));

		pipeline.AppendStage(m_transparencyStage);
	}


	void TransparencyGraphicsSystem::PreRender()
	{
		SEAssert(m_ambientIEMTex && m_ambientPMREMTex && m_ambientParams,
			"Required inputs are null: We should at least have received any empty pointer");

		// Early out:
		const gr::RenderDataID mainCamID = m_graphicsSystemManager->GetActiveCameraRenderDataID();
		if ((m_viewBatches == nullptr || 
				mainCamID == gr::k_invalidRenderDataID ||
				m_viewBatches->at(mainCamID).empty()) &&
			(m_allBatches == nullptr || m_allBatches->empty()))
		{
			return;
		}

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		gr::IndexedBufferManager& ibm = renderData.GetInstancingIndexedBufferManager();

		// Add our inputs each frame in case the light changes/they're updated by the source GS
		if (m_ambientIEMTex && m_ambientPMREMTex && *m_ambientParams)
		{
			m_transparencyStage->AddSingleFrameTextureInput(
				"CubeMapIEM",
				*m_ambientIEMTex,
				m_graphicsSystemManager->GetSampler("WrapMinMagMipLinear"),
				re::TextureView(*m_ambientIEMTex));

			m_transparencyStage->AddSingleFrameTextureInput(
				"CubeMapPMREM",
				*m_ambientPMREMTex,
				m_graphicsSystemManager->GetSampler("WrapMinMagMipLinear"),
				re::TextureView(*m_ambientPMREMTex));

			m_transparencyStage->AddSingleFrameBuffer(AmbientLightData::s_shaderName, *m_ambientParams);
		}
		else
		{
			m_transparencyStage->AddSingleFrameBuffer(
				AmbientLightData::s_shaderName,
				re::Buffer::Create(
					AmbientLightData::s_shaderName,
					grutil::GetAmbientLightData(
						1,
						0.f,
						0.f,
						static_cast<uint32_t>(core::Config::GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey)),
						nullptr),
					re::Buffer::BufferParams{
						.m_lifetime = re::Lifetime::SingleFrame,
						.m_stagingPool = re::Buffer::StagingPool::Temporary,
						.m_memPoolPreference = re::Buffer::UploadHeap,
						.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
						.m_usageMask = re::Buffer::Constant,
					}));
		}

		// Shadow texture arrays:
		m_transparencyStage->AddSingleFrameTextureInput(
			"DirectionalShadows",
			*m_directionalShadowArrayTex,
			m_graphicsSystemManager->GetSampler("BorderCmpMinMagLinearMipPoint"),
			re::TextureView(*m_directionalShadowArrayTex, {re::TextureView::ViewFlags::ReadOnlyDepth}));

		m_transparencyStage->AddSingleFrameTextureInput(
			"PointShadows",
			*m_pointShadowArrayTex,
			m_graphicsSystemManager->GetSampler("WrapCmpMinMagLinearMipPoint"),
			re::TextureView(*m_pointShadowArrayTex, { re::TextureView::ViewFlags::ReadOnlyDepth }));

		m_transparencyStage->AddSingleFrameTextureInput(
			"SpotShadows",
			*m_spotShadowArrayTex,
			m_graphicsSystemManager->GetSampler("BorderCmpMinMagLinearMipPoint"),
			re::TextureView(*m_spotShadowArrayTex, { re::TextureView::ViewFlags::ReadOnlyDepth }));


		// Indexed light data buffers:
		auto PrePopulateLightShadowLUTData = [this](
			std::span<const gr::RenderDataID> const& lightIDs,
			gr::Light::Type lightType) -> std::vector<LightShadowLUTData>
			{
				std::vector<LightShadowLUTData> partialLUTData;
				partialLUTData.resize(lightIDs.size());

				for (size_t i = 0; i < lightIDs.size(); ++i)
				{
					uint32_t shadowTexArrayIdx = INVALID_SHADOW_IDX;
					if (m_lightIDToShadowRecords)
					{
						auto shadowRecordItr = m_lightIDToShadowRecords->find(lightIDs[i]);
						shadowTexArrayIdx = shadowRecordItr == m_lightIDToShadowRecords->end() ?
							INVALID_SHADOW_IDX : shadowRecordItr->second.m_shadowTexArrayIdx;
					}

					partialLUTData[i].g_lightShadowIdx.z = shadowTexArrayIdx;
					partialLUTData[i].g_lightShadowIdx.w = static_cast<uint32_t>(lightType);
				}

				return partialLUTData;
			};

		
		// Directional light buffer:
		m_transparencyStage->AddSingleFrameBuffer(ibm.GetIndexedBufferInput(
			LightData::s_directionalLightDataShaderName, LightData::s_directionalLightDataShaderName));

		// Get the directional light RenderDataIDs: We assume directional lights are always visible/never culled
		std::span<const gr::RenderDataID> const& directionalIDs =
			renderData.GetRegisteredRenderDataIDsSpan<gr::Light::RenderDataDirectional>();

		// Directional light buffer LUT:
		m_transparencyStage->AddSingleFrameBuffer(
			ibm.GetLUTBufferInput<LightShadowLUTData>(
				LightShadowLUTData::s_shaderNameDirectional,
				PrePopulateLightShadowLUTData(directionalIDs, gr::Light::Type::Directional),
				directionalIDs));

		// Point light buffer:
		m_transparencyStage->AddSingleFrameBuffer(ibm.GetIndexedBufferInput(
			LightData::s_pointLightDataShaderName, LightData::s_pointLightDataShaderName));

		// Point light buffer LUT:
		m_transparencyStage->AddSingleFrameBuffer(
			ibm.GetLUTBufferInput<LightShadowLUTData>(
				LightShadowLUTData::s_shaderNamePoint,
				PrePopulateLightShadowLUTData(*m_pointCullingResults, gr::Light::Type::Point),
				*m_pointCullingResults));

		// Spot light buffer:
		m_transparencyStage->AddSingleFrameBuffer(ibm.GetIndexedBufferInput(
			LightData::s_spotLightDataShaderName, LightData::s_spotLightDataShaderName));

		// Spot light buffer LUT:
		m_transparencyStage->AddSingleFrameBuffer(
			ibm.GetLUTBufferInput<LightShadowLUTData>(
				LightShadowLUTData::s_shaderNameSpot,
				PrePopulateLightShadowLUTData(*m_spotCullingResults, gr::Light::Type::Spot),
				*m_spotCullingResults));

		// Indexed shadows:
		m_transparencyStage->AddSingleFrameBuffer(
			ibm.GetIndexedBufferInput(ShadowData::s_shaderName, ShadowData::s_shaderName));

		// Light/shadow metadata (i.e. Light counts):
		CreateUpdateLightMetadata(
			m_lightMetadata, m_lightMetadataBuffer, renderData, m_pointCullingResults, m_spotCullingResults, "LightCounts");
		
		m_transparencyStage->AddSingleFrameBuffer(m_lightMetadataBuffer);

		
		// Finally, add the geometry batches:
		if (m_viewBatches && mainCamID != gr::k_invalidRenderDataID)
		{
			SEAssert(m_viewBatches->contains(mainCamID), "Cannot find main camera ID in view batches");
			m_transparencyStage->AddBatches(m_viewBatches->at(mainCamID));
		}
		else
		{
			SEAssert(m_allBatches, "Must have all batches if view batches is null");
			m_transparencyStage->AddBatches(*m_allBatches);
		}
	}
}