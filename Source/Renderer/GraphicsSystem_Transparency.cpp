// © 2024 Adam Badke. All rights reserved.
#include "GraphicsSystem_Transparency.h"
#include "GraphicsSystemManager.h"
#include "LightParamsHelpers.h"
#include "RenderManager.h"
#include "Sampler.h"

#include "Core/Config.h"

#include "Shaders/Common/LightParams.h"


namespace
{
	re::BufferInput CreateAllLightIndexesBuffer(
		gr::RenderDataManager const& renderData,
		gr::PunctualLightCullingResults const* pointCullingIDs,
		gr::PunctualLightCullingResults const* spotCullingIDs,
		gr::LightDataBufferIdxMap const* pointLightDataBufferIdxMap,
		gr::LightDataBufferIdxMap const* spotLightDataBufferIdxMap,
		char const* bufferName)
	{
		const uint32_t numDirectional = renderData.GetNumElementsOfType<gr::Light::RenderDataDirectional>();
		const uint32_t numPoint = pointCullingIDs ? util::CheckedCast<uint32_t>(pointCullingIDs->size()) : 0;
		const uint32_t numSpot = spotCullingIDs ? util::CheckedCast<uint32_t>(spotCullingIDs->size()) : 0;

		SEAssert(numDirectional < AllLightIndexesData::k_maxLights && 
			numPoint < AllLightIndexesData::k_maxLights &&
			numSpot < AllLightIndexesData::k_maxLights,
			"Too many lights to pack into fixed size array");

		// Note: We (currently) assume that all directional lights will contribute all the time

		AllLightIndexesData allLightIndexesData{};

		uint32_t contributingPoint = 0;
		for (uint32_t lightIdx = 0; lightIdx < numPoint; ++lightIdx)
		{
			const gr::RenderDataID pointID = pointCullingIDs->at(lightIdx);

			gr::Light::RenderDataPoint const& pointData =
				renderData.GetObjectData<gr::Light::RenderDataPoint>(pointID);

			if (pointData.m_canContribute)
			{
				PackAllLightIndexesDataValue(
					allLightIndexesData,
					gr::Light::Point,
					contributingPoint++,
					gr::GetLightDataBufferIdx(pointLightDataBufferIdxMap, pointID));
			}
		}

		uint32_t contributingSpot = 0;
		for (uint32_t lightIdx = 0; lightIdx < numSpot; ++lightIdx)
		{
			const gr::RenderDataID spotID = spotCullingIDs->at(lightIdx);

			gr::Light::RenderDataSpot const& spotData =
				renderData.GetObjectData<gr::Light::RenderDataSpot>(spotID);

			if (spotData.m_canContribute)
			{
				PackAllLightIndexesDataValue(
					allLightIndexesData,
					gr::Light::Spot,
					contributingSpot++,
					gr::GetLightDataBufferIdx(spotLightDataBufferIdxMap, spotID));
			}
		}

		allLightIndexesData.g_numLights = glm::uvec4(
			numDirectional,
			contributingPoint,
			contributingSpot,
			0);

		return re::BufferInput(
			bufferName,
			re::Buffer::Create(
				bufferName,
				allLightIndexesData, 
				re::Buffer::BufferParams{
					.m_lifetime = re::Lifetime::SingleFrame,
					.m_stagingPool = re::Buffer::StagingPool::Temporary,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Constant,
				}));
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
		, m_directionalLightDataBuffer(nullptr)
		, m_pointLightDataBuffer(nullptr)
		, m_spotLightDataBuffer(nullptr)
		, m_pointLightDataBufferIdxMap(nullptr)
		, m_spotLightDataBufferIdxMap(nullptr)
		, m_directionalShadowArrayTex(nullptr)
		, m_pointShadowArrayTex(nullptr)
		, m_spotShadowArrayTex(nullptr)
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

		RegisterBufferInput(k_directionalLightDataBufferInput);
		RegisterBufferInput(k_pointLightDataBufferInput);
		RegisterBufferInput(k_spotLightDataBufferInput);

		RegisterDataInput(k_IDToPointIdxDataInput);
		RegisterDataInput(k_IDToSpotIdxDataInput);

		RegisterTextureInput(k_directionalShadowArrayTexInput);
		RegisterTextureInput(k_pointShadowArrayTexInput);
		RegisterTextureInput(k_spotShadowArrayTexInput);

		RegisterBufferInput(k_PCSSSampleParamsBufferInput);
	}


	void TransparencyGraphicsSystem::RegisterOutputs()
	{
		//
	}


	void TransparencyGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
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

		m_directionalLightDataBuffer = bufferDependencies.at(k_directionalLightDataBufferInput);
		m_pointLightDataBuffer = bufferDependencies.at(k_pointLightDataBufferInput);
		m_spotLightDataBuffer = bufferDependencies.at(k_spotLightDataBufferInput);

		m_pointLightDataBufferIdxMap = GetDataDependency<LightDataBufferIdxMap>(k_IDToPointIdxDataInput, dataDependencies);
		m_spotLightDataBufferIdxMap = GetDataDependency<LightDataBufferIdxMap>(k_IDToSpotIdxDataInput, dataDependencies);

		m_directionalShadowArrayTex = texDependencies.at(k_directionalShadowArrayTexInput);
		m_pointShadowArrayTex = texDependencies.at(k_pointShadowArrayTexInput);
		m_spotShadowArrayTex = texDependencies.at(k_spotShadowArrayTexInput);

		m_PCSSSampleParamsBuffer = bufferDependencies.at(k_PCSSSampleParamsBufferInput);


		// Stage setup:
		m_transparencyStage = re::Stage::CreateGraphicsStage("Transparency Stage", {});

		m_transparencyStage->SetBatchFilterMaskBit(
			re::Batch::Filter::AlphaBlended, re::Stage::FilterMode::Require, true);

		m_transparencyStage->SetDrawStyle(effect::drawstyle::RenderPath_Forward);

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
			re::Sampler::GetSampler("ClampMinMagMipPoint"),
			re::TextureView(*texDependencies.at(k_ambientDFGTexInput)));

		pipeline.AppendRenderStage(m_transparencyStage);
	}


	void TransparencyGraphicsSystem::PreRender()
	{
		SEAssert(m_ambientIEMTex && m_ambientPMREMTex && m_ambientParams,
			"Required inputs are null: We should at least have received any empty pointer");

		// Add our inputs each frame in case the light changes/they're updated by the source GS
		if (m_ambientIEMTex && m_ambientPMREMTex && *m_ambientParams)
		{
			m_transparencyStage->AddSingleFrameTextureInput(
				"CubeMapIEM",
				*m_ambientIEMTex,
				re::Sampler::GetSampler("WrapMinMagMipLinear"),
				re::TextureView(*m_ambientIEMTex));

			m_transparencyStage->AddSingleFrameTextureInput(
				"CubeMapPMREM",
				*m_ambientPMREMTex,
				re::Sampler::GetSampler("WrapMinMagMipLinear"),
				re::TextureView(*m_ambientPMREMTex));

			m_transparencyStage->AddSingleFrameBuffer(AmbientLightData::s_shaderName, *m_ambientParams);
		}
		else
		{
			m_transparencyStage->AddSingleFrameBuffer(
				AmbientLightData::s_shaderName,
				re::Buffer::Create(
					AmbientLightData::s_shaderName,
					GetAmbientLightParamsData(
						1,
						0.f,
						0.f,
						static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey)),
						nullptr),
					re::Buffer::BufferParams{
						.m_lifetime = re::Lifetime::SingleFrame,
						.m_stagingPool = re::Buffer::StagingPool::Temporary,
						.m_memPoolPreference = re::Buffer::UploadHeap,
						.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
						.m_usageMask = re::Buffer::Constant,
					}));
		}

		// Punctual light buffers:
		m_transparencyStage->AddSingleFrameBuffer(LightData::s_directionalLightDataShaderName, *m_directionalLightDataBuffer);
		m_transparencyStage->AddSingleFrameBuffer(LightData::s_pointLightDataShaderName, *m_pointLightDataBuffer);
		m_transparencyStage->AddSingleFrameBuffer(LightData::s_spotLightDataShaderName, *m_spotLightDataBuffer);
		
		m_transparencyStage->AddSingleFrameBuffer(CreateAllLightIndexesBuffer(
			m_graphicsSystemManager->GetRenderData(),
			m_pointCullingResults,
			m_spotCullingResults,
			m_pointLightDataBufferIdxMap,
			m_spotLightDataBufferIdxMap,
			"AllLightIndexesParams"));

		// Shadow texture arrays:
		m_transparencyStage->AddSingleFrameTextureInput(
			"DirectionalShadows",
			*m_directionalShadowArrayTex,
			re::Sampler::GetSampler("BorderCmpMinMagLinearMipPoint"),
			re::TextureView(*m_directionalShadowArrayTex, {re::TextureView::ViewFlags::ReadOnlyDepth}));

		m_transparencyStage->AddSingleFrameTextureInput(
			"PointShadows",
			*m_pointShadowArrayTex,
			re::Sampler::GetSampler("WrapCmpMinMagLinearMipPoint"),
			re::TextureView(*m_pointShadowArrayTex, { re::TextureView::ViewFlags::ReadOnlyDepth }));

		m_transparencyStage->AddSingleFrameTextureInput(
			"SpotShadows",
			*m_spotShadowArrayTex,
			re::Sampler::GetSampler("BorderCmpMinMagLinearMipPoint"),
			re::TextureView(*m_spotShadowArrayTex, { re::TextureView::ViewFlags::ReadOnlyDepth }));

		const gr::RenderDataID mainCamID = m_graphicsSystemManager->GetActiveCameraRenderDataID();
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