// © 2024 Adam Badke. All rights reserved.
#include "BatchManager.h"
#include "GraphicsSystem_Transparency.h"
#include "GraphicsSystemManager.h"
#include "LightParamsHelpers.h"
#include "RenderManager.h"
#include "Sampler.h"

#include "Core/Config.h"

#include "Shaders/Common/LightParams.h"


namespace
{
	std::shared_ptr<re::Buffer> CreateAllLightIndexesBuffer(
		gr::RenderDataManager const& renderData,
		gr::LightManager const& lightManager,
		gr::GraphicsSystem::PunctualLightCullingResults const* pointCullingIDs,
		gr::GraphicsSystem::PunctualLightCullingResults const* spotCullingIDs,
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
					lightManager.GetLightDataBufferIdx(gr::Light::Point, pointID));
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
					lightManager.GetLightDataBufferIdx(gr::Light::Spot, spotID));
			}
		}

		allLightIndexesData.g_numLights = glm::uvec4(
			numDirectional,
			contributingPoint,
			contributingSpot,
			0);

		return re::Buffer::Create(bufferName, allLightIndexesData, re::Buffer::Type::SingleFrame);
	}
}

namespace gr
{
	TransparencyGraphicsSystem::TransparencyGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_ambientIEMTex(nullptr)
		, m_ambientPMREMTex(nullptr)
		, m_ambientParams(nullptr)
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

		RegisterDataInput(k_viewCullingDataInput);
		RegisterDataInput(k_pointLightCullingDataInput);
		RegisterDataInput(k_spotLightCullingDataInput);
	}


	void TransparencyGraphicsSystem::RegisterOutputs()
	{
		//
	}


	void TransparencyGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const& bufferDependencies)
	{
		SEAssert(texDependencies.contains(k_ambientIEMTexInput) &&
			texDependencies.at(k_ambientIEMTexInput) != nullptr &&
			texDependencies.contains(k_ambientPMREMTexInput) &&
			texDependencies.at(k_ambientPMREMTexInput) != nullptr &&
			texDependencies.contains(k_sceneLightingTexInput) &&
			texDependencies.at(k_sceneLightingTexInput) != nullptr &&
			texDependencies.contains(k_ambientDFGTexInput) &&
			texDependencies.at(k_ambientDFGTexInput) != nullptr &&
			bufferDependencies.contains(k_ambientParamsBufferInput) &&
			bufferDependencies.at(k_ambientParamsBufferInput) != nullptr,
			"Missing a required input: We should at least receive some defaults");

		m_transparencyStage = re::RenderStage::CreateGraphicsStage("Transparency Stage", {});

		m_transparencyStage->SetBatchFilterMaskBit(
			re::Batch::Filter::AlphaBlended, re::RenderStage::FilterMode::Require, true);

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

		transparencyTarget->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
			re::TextureTarget::BlendMode::SrcAlpha,
			re::TextureTarget::BlendMode::OneMinusSrcAlpha });

		m_transparencyStage->SetTextureTargetSet(transparencyTarget);

		// Buffers:		
		m_transparencyStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());
		m_transparencyStage->AddPermanentBuffer(transparencyTarget->GetCreateTargetParamsBuffer());

		m_transparencyStage->AddPermanentBuffer(
			re::RenderManager::Get()->GetLightManager().GetPCSSPoissonSampleParamsBuffer());

		// Texture inputs:
		m_transparencyStage->AddPermanentTextureInput(
			"DFG",
			texDependencies.at(k_ambientDFGTexInput)->get(),
			re::Sampler::GetSampler("ClampMinMagMipPoint").get(),
			re::TextureView(texDependencies.at(k_ambientDFGTexInput)->get()));

		// Cache the pointers
		m_ambientIEMTex = texDependencies.at(k_ambientIEMTexInput);
		m_ambientPMREMTex = texDependencies.at(k_ambientPMREMTexInput);
		m_ambientParams = bufferDependencies.at(k_ambientParamsBufferInput);

		pipeline.AppendRenderStage(m_transparencyStage);
	}


	void TransparencyGraphicsSystem::PreRender(DataDependencies const& dataDependencies)
	{
		SEAssert(m_ambientIEMTex && m_ambientPMREMTex && m_ambientParams,
			"Required inputs are null: We should at least have received any empty pointer");

		// Add our inputs each frame in case the light changes/they're updated by the source GS
		if (*m_ambientIEMTex && *m_ambientPMREMTex && *m_ambientParams)
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

			m_transparencyStage->AddSingleFrameBuffer(*m_ambientParams);
		}
		else
		{
			m_transparencyStage->AddSingleFrameBuffer(re::Buffer::Create(
				AmbientLightData::s_shaderName,
				GetAmbientLightParamsData(
					1,
					0.f,
					0.f,
					static_cast<uint32_t>(core::Config::Get()->GetValue<int>(core::configkeys::k_brdfLUTWidthHeightKey)),
					nullptr),
				re::Buffer::Type::SingleFrame));
		}

		// Punctual light buffers:
		gr::LightManager const& lightManager = re::RenderManager::Get()->GetLightManager();

		m_transparencyStage->AddSingleFrameBuffer(lightManager.GetLightDataBuffer(gr::Light::Directional));
		m_transparencyStage->AddSingleFrameBuffer(lightManager.GetLightDataBuffer(gr::Light::Point));
		m_transparencyStage->AddSingleFrameBuffer(lightManager.GetLightDataBuffer(gr::Light::Spot));
		
		m_transparencyStage->AddSingleFrameBuffer(CreateAllLightIndexesBuffer(
			m_graphicsSystemManager->GetRenderData(),
			lightManager,
			static_cast<PunctualLightCullingResults const*>(dataDependencies.at(k_pointLightCullingDataInput)),
			static_cast<PunctualLightCullingResults const*>(dataDependencies.at(k_spotLightCullingDataInput)),
			"AllLightIndexesParams"));

		// Shadow texture arrays:
		std::shared_ptr<re::Texture> directionalShadowArray = lightManager.GetShadowArrayTexture(gr::Light::Directional);
		m_transparencyStage->AddSingleFrameTextureInput(
			"DirectionalShadows",
			directionalShadowArray,
			re::Sampler::GetSampler("BorderCmpMinMagLinearMipPoint"),
			re::TextureView(directionalShadowArray, {re::TextureView::ViewFlags::ReadOnlyDepth}));

		std::shared_ptr<re::Texture> pointShadowArray = lightManager.GetShadowArrayTexture(gr::Light::Point);
		m_transparencyStage->AddSingleFrameTextureInput(
			"PointShadows",
			pointShadowArray,
			re::Sampler::GetSampler("WrapCmpMinMagLinearMipPoint"),
			re::TextureView(pointShadowArray, { re::TextureView::ViewFlags::ReadOnlyDepth }));

		std::shared_ptr<re::Texture> spotShadowArray = lightManager.GetShadowArrayTexture(gr::Light::Spot);
		m_transparencyStage->AddSingleFrameTextureInput(
			"SpotShadows",
			spotShadowArray,
			re::Sampler::GetSampler("BorderCmpMinMagLinearMipPoint"),
			re::TextureView(spotShadowArray, { re::TextureView::ViewFlags::ReadOnlyDepth }));


		gr::BatchManager const& batchMgr = m_graphicsSystemManager->GetBatchManager();

		ViewCullingResults const* viewCullingResults =
			static_cast<ViewCullingResults const*>(dataDependencies.at(k_viewCullingDataInput));

		if (viewCullingResults)
		{
			const gr::RenderDataID mainCamID = m_graphicsSystemManager->GetActiveCameraRenderDataID();

			std::vector<re::Batch> const& sceneBatches = batchMgr.GetSceneBatches(
				viewCullingResults->at(mainCamID),
				(gr::BatchManager::InstanceType::Transform | gr::BatchManager::InstanceType::Material),
				re::Batch::Filter::AlphaBlended);

			m_transparencyStage->AddBatches(sceneBatches);
		}
		else
		{
			std::vector<re::Batch> const& allSceneBatches = batchMgr.GetAllSceneBatches(
				(gr::BatchManager::InstanceType::Transform | gr::BatchManager::InstanceType::Material),
				re::Batch::Filter::AlphaBlended);

			m_transparencyStage->AddBatches(allSceneBatches);
		}
	}
}