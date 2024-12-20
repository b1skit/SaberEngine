// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystemManager.h"
#include "LightRenderData.h"
#include "RenderManager.h"
#include "RenderDataManager.h"
#include "ShadowMapRenderData.h"

#include "Core/Config.h"
#include "Core/InvPtr.h"

#include "Shaders/Common/ShadowRenderParams.h"


namespace
{
	CubemapShadowRenderData GetCubemapShadowRenderParamsData(
		gr::Camera::RenderData const& shadowCamData, gr::Transform::RenderData const& transformData)
	{
		SEAssert(shadowCamData.m_cameraConfig.m_projectionType == gr::Camera::Config::ProjectionType::PerspectiveCubemap,
			"Invalid projection type");

		CubemapShadowRenderData cubemapShadowParams{};
		
		std::vector<glm::mat4> const& cubeViewMatrices = 
			gr::Camera::BuildAxisAlignedCubeViewMatrices(transformData.m_globalPosition);

		for (uint8_t faceIdx = 0; faceIdx < 6; faceIdx++)
		{
			cubemapShadowParams.g_cubemapShadowCam_VP[faceIdx] = 
				shadowCamData.m_cameraParams.g_projection * cubeViewMatrices[faceIdx];
		}
		
		cubemapShadowParams.g_cubemapShadowCamNearFar = 
			glm::vec4(shadowCamData.m_cameraConfig.m_near, shadowCamData.m_cameraConfig.m_far, 0.f, 0.f);

		cubemapShadowParams.g_cubemapLightWorldPos = glm::vec4(transformData.m_globalPosition, 0.f);

		return cubemapShadowParams;
	}


	re::TextureView CreateShadowWriteView(gr::Light::Type lightType, uint32_t shadowArrayIdx)
	{
		switch (lightType)
		{
		case gr::Light::Directional: return re::TextureView(re::TextureView::Texture2DArrayView{0, 1, shadowArrayIdx, 1});
		case gr::Light::Point: return re::TextureView(re::TextureView::Texture2DArrayView{0, 1, shadowArrayIdx * 6, 6});
		case gr::Light::Spot: return re::TextureView(re::TextureView::Texture2DArrayView{0, 1, shadowArrayIdx, 1});
		case gr::Light::AmbientIBL:
		default: SEAssertF("Invalid light type");
		}
		return re::TextureView(re::TextureView::Texture2DArrayView{0, 1, shadowArrayIdx, 1 }); // This should never happen
	}


	re::Viewport CreateShadowArrayWriteViewport(core::InvPtr<re::Texture> const& shadowArray)
	{
		return re::Viewport(0, 0, shadowArray->Width(), shadowArray->Height());
	}


	re::ScissorRect CreateShadowArrayWriteScissorRect(core::InvPtr<re::Texture> const& shadowArray)
	{
		return re::ScissorRect(0, 0, static_cast<long>(shadowArray->Width()), static_cast<long>(shadowArray->Height()));
	}
}


namespace gr
{
	ShadowsGraphicsSystem::ShadowsGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_stagePipeline(nullptr)
		, m_pointCullingResults(nullptr)
		, m_spotCullingResults(nullptr)
		, m_directionalShadowArrayTex(nullptr)
		, m_pointShadowArrayTex(nullptr)
		, m_spotShadowArrayTex(nullptr)
		, m_directionalShadowArrayIdxMap(nullptr)
		, m_pointShadowArrayIdxMap(nullptr)
		, m_spotShadowArrayIdxMap(nullptr)
	{
	}


	void ShadowsGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_pointLightCullingDataInput);
		RegisterDataInput(k_spotLightCullingDataInput);

		RegisterDataInput(k_viewBatchesDataInput);
		RegisterDataInput(k_allBatchesDataInput);

		RegisterTextureInput(k_directionalShadowArrayTexInput);
		RegisterTextureInput(k_pointShadowArrayTexInput);
		RegisterTextureInput(k_spotShadowArrayTexInput);

		RegisterDataInput(k_IDToDirectionalShadowArrayIdxDataInput);
		RegisterDataInput(k_IDToPointShadowArrayIdxDataInput);
		RegisterDataInput(k_IDToSpotShadowArrayIdxDataInput);
	}


	void ShadowsGraphicsSystem::RegisterOutputs()
	{
		//
	}


	void ShadowsGraphicsSystem::CreateRegisterCubeShadowStage(
		std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
		gr::RenderDataID lightID,
		gr::ShadowMap::RenderData const& shadowData,
		gr::Transform::RenderData const& transformData,
		gr::Camera::RenderData const& camData)
	{
		char const* lightName = shadowData.m_owningLightName;
		std::string const& stageName = std::format("{}_CubeShadow", lightName);

		std::shared_ptr<re::RenderStage> shadowStage =
			re::RenderStage::CreateGraphicsStage(stageName.c_str(), re::RenderStage::GraphicsStageParams{});

		shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::CastsShadow, re::RenderStage::FilterMode::Require, true);
		shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::AlphaBlended, re::RenderStage::FilterMode::Exclude, true);

		shadowStage->SetDrawStyle(effect::drawstyle::Shadow_Cube);
		
		SEAssert(shadowData.m_lightType == gr::Light::Point, "Unexpected light type for a cube stage");

		// Texture target set:
		std::shared_ptr<re::TextureTargetSet> pointShadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_CubeShadowTargetSet", lightName));
		
		pointShadowTargetSet->SetDepthStencilTarget(
			*m_pointShadowArrayTex,
			re::TextureTarget::TargetParams{
				.m_textureView = CreateShadowWriteView(
					shadowData.m_lightType, 
					GetShadowArrayIdx(m_pointShadowArrayIdxMap, lightID)) });

		pointShadowTargetSet->SetViewport(CreateShadowArrayWriteViewport(*m_pointShadowArrayTex));
		pointShadowTargetSet->SetScissorRect(CreateShadowArrayWriteScissorRect(*m_pointShadowArrayTex));
		pointShadowTargetSet->SetDepthTargetClearMode(re::TextureTarget::ClearMode::Enabled);

		shadowStage->SetTextureTargetSet(pointShadowTargetSet);

		// Cubemap shadow buffer:
		CubemapShadowRenderData const& cubemapShadowParams = GetCubemapShadowRenderParamsData(camData, transformData);

		re::BufferInput cubeShadowBuf(
			CubemapShadowRenderData::s_shaderName,
			re::Buffer::Create(
				CubemapShadowRenderData::s_shaderName,
				cubemapShadowParams,
				re::Buffer::BufferParams{
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Constant,
				}));

		shadowStage->AddPermanentBuffer(cubeShadowBuf);

		dstStageData.emplace(
			lightID,
			ShadowStageData{
				.m_renderStage = shadowStage,
				.m_shadowTargetSet = pointShadowTargetSet,
				.m_shadowCamParamBlock = cubeShadowBuf });
	}


	void ShadowsGraphicsSystem::CreateRegister2DShadowStage(
		std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
		gr::RenderDataID lightID,
		gr::ShadowMap::RenderData const& shadowData,
		gr::Camera::RenderData const& shadowCamData)
	{
		char const* lightName = shadowData.m_owningLightName;
		std::string const& stageName = std::format("{}_2DShadow", lightName);

		std::shared_ptr<re::RenderStage> shadowStage =
			re::RenderStage::CreateGraphicsStage(stageName.c_str(), re::RenderStage::GraphicsStageParams{});

		shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::CastsShadow, re::RenderStage::FilterMode::Require, true);

		// Shadow camera buffer:
		re::BufferInput shadowCamParams(
			CameraData::s_shaderName,
			re::Buffer::Create(
				CameraData::s_shaderName,
				shadowCamData.m_cameraParams,
				re::Buffer::BufferParams{
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Constant,
				}));

		shadowStage->AddPermanentBuffer(shadowCamParams);

		shadowStage->SetDrawStyle(effect::drawstyle::Shadow_2D);

		core::InvPtr<re::Texture> shadowArrayTex;
		ShadowArrayIdxMap const* shadowArrayIdxMap = nullptr;
		switch (shadowData.m_lightType)
		{
		case gr::Light::Type::Directional:
		{
			shadowArrayTex = *m_directionalShadowArrayTex;
			shadowArrayIdxMap = m_directionalShadowArrayIdxMap;
		}
		break;
		case gr::Light::Type::Spot:
		{
			shadowArrayTex = *m_spotShadowArrayTex;
			shadowArrayIdxMap = m_spotShadowArrayIdxMap;
		}
		break;
		default: SEAssertF("Invalid light type");
		}

		// Texture target set:
		std::shared_ptr<re::TextureTargetSet> shadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_2DShadowTargetSet", lightName));
		
		shadowTargetSet->SetDepthStencilTarget(
			shadowArrayTex, 
			re::TextureTarget::TargetParams{
				.m_textureView = CreateShadowWriteView(
					shadowData.m_lightType,
					GetShadowArrayIdx(shadowArrayIdxMap, lightID)) });

		shadowTargetSet->SetViewport(CreateShadowArrayWriteViewport(shadowArrayTex));;
		shadowTargetSet->SetScissorRect(CreateShadowArrayWriteScissorRect(shadowArrayTex));	

		shadowTargetSet->SetDepthTargetClearMode(re::TextureTarget::ClearMode::Enabled);

		shadowStage->SetTextureTargetSet(shadowTargetSet);

		dstStageData.emplace(
			lightID,
			ShadowStageData{
				.m_renderStage = shadowStage,
				.m_shadowTargetSet = shadowTargetSet,
				.m_shadowCamParamBlock = shadowCamParams });
	}


	void ShadowsGraphicsSystem::InitPipeline(
		re::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const&,
		DataDependencies const& dataDependencies)
	{
		m_stagePipeline = &pipeline;

		std::shared_ptr<re::RenderStage> directionalParentStage = 
			re::RenderStage::CreateParentStage("Directional shadow stages");
		m_directionalParentStageItr = pipeline.AppendRenderStage(directionalParentStage);

		std::shared_ptr<re::RenderStage> pointParentStage = re::RenderStage::CreateParentStage("Point shadow stages");
		m_pointParentStageItr = pipeline.AppendRenderStage(pointParentStage);

		std::shared_ptr<re::RenderStage> spotParentStage = re::RenderStage::CreateParentStage("Spot shadow stages");
		m_spotParentStageItr = pipeline.AppendRenderStage(spotParentStage);

		// Cache our dependencies:
		m_pointCullingResults = GetDataDependency<PunctualLightCullingResults>(k_pointLightCullingDataInput, dataDependencies);
		m_spotCullingResults = GetDataDependency<PunctualLightCullingResults>(k_spotLightCullingDataInput, dataDependencies);

		m_viewBatches = GetDataDependency<ViewBatches>(k_viewBatchesDataInput, dataDependencies);
		m_allBatches = GetDataDependency<AllBatches>(k_allBatchesDataInput, dataDependencies);
		SEAssert(m_viewBatches || m_allBatches, "Must have received some batches");

		m_directionalShadowArrayTex = texDependencies.at(k_directionalShadowArrayTexInput);
		m_pointShadowArrayTex = texDependencies.at(k_pointShadowArrayTexInput);
		m_spotShadowArrayTex = texDependencies.at(k_spotShadowArrayTexInput);

		m_directionalShadowArrayIdxMap = GetDataDependency<ShadowArrayIdxMap>(k_IDToDirectionalShadowArrayIdxDataInput, dataDependencies);
		m_pointShadowArrayIdxMap = GetDataDependency<ShadowArrayIdxMap>(k_IDToPointShadowArrayIdxDataInput, dataDependencies);
		m_spotShadowArrayIdxMap = GetDataDependency<ShadowArrayIdxMap>(k_IDToSpotShadowArrayIdxDataInput, dataDependencies);
	}


	void ShadowsGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Delete removed deleted lights:
		auto DeleteLights = [&](
			std::vector<gr::RenderDataID> const* deletedIDs,
			std::unordered_map<gr::RenderDataID, ShadowStageData>& stageData)
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
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataDirectional>(), m_directionalShadowStageData);
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataPoint>(), m_pointShadowStageData);
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataSpot>(), m_spotShadowStageData);

		// Register new directional and spot lights:
		auto Register2DShadowLights = [&](std::vector<gr::RenderDataID> const* newLightIDs,
			std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData)
		{
			if (!newLightIDs)
			{
				return;
			}

			auto lightItr = renderData.IDBegin(*newLightIDs);
			auto const& lightItrEnd = renderData.IDEnd(*newLightIDs);
			while (lightItr != lightItrEnd)
			{
				if (lightItr.HasObjectData<gr::ShadowMap::RenderData>())
				{
					SEAssert(lightItr.HasObjectData<gr::Camera::RenderData>(),
						"Shadow map and shadow camera render data are both required for shadows");

					CreateRegister2DShadowStage(
						dstStageData,
						lightItr.GetRenderDataID(),
						lightItr.Get<gr::ShadowMap::RenderData>(),
						lightItr.Get<gr::Camera::RenderData>());
				}
				++lightItr;
			}
		};
		Register2DShadowLights(renderData.GetIDsWithNewData<gr::Light::RenderDataDirectional>(), m_directionalShadowStageData);
		Register2DShadowLights(renderData.GetIDsWithNewData<gr::Light::RenderDataSpot>(), m_spotShadowStageData);

		// Register new point lights:
		std::vector<gr::RenderDataID> const* newPointIDs =
			renderData.GetIDsWithNewData<gr::Light::RenderDataPoint>();
		if (newPointIDs)
		{
			auto pointItr = renderData.IDBegin(*newPointIDs);
			auto const& pointItrEnd = renderData.IDEnd(*newPointIDs);
			while (pointItr != pointItrEnd)
			{
				if (pointItr.HasObjectData<gr::ShadowMap::RenderData>())
				{
					SEAssert(pointItr.HasObjectData<gr::Camera::RenderData>(),
						"Shadow map and shadow camera render data are both required for shadows");

					CreateRegisterCubeShadowStage(
						m_pointShadowStageData,
						pointItr.GetRenderDataID(),
						pointItr.Get<gr::ShadowMap::RenderData>(),
						pointItr.GetTransformData(),
						pointItr.Get<gr::Camera::RenderData>());
				}
				++pointItr;
			}
		}


		// Update directional and spot shadow buffers, if necessary:
		auto Update2DShadowCamData = [](
			gr::Light::Type lightType,
			gr::RenderDataManager::IDIterator<std::vector<gr::RenderDataID>> const& lightItr,
			std::unordered_map<gr::RenderDataID, ShadowStageData>& stageData,
			bool hasShadow)
			{
				SEAssert(hasShadow == false ||
					(lightItr.HasObjectData<gr::Camera::RenderData>() &&
						lightItr.HasObjectData<gr::ShadowMap::RenderData>()),
					"If a light has a shadow, it must have a shadow camera");

				if (hasShadow)
				{
					const gr::RenderDataID lightID = lightItr.GetRenderDataID();

					ShadowStageData& shadowStageData = stageData.at(lightID);

					if (lightItr.IsDirty<gr::Camera::RenderData>() || lightItr.TransformIsDirty())
					{
						gr::Camera::RenderData const& shadowCamData = lightItr.Get<gr::Camera::RenderData>();

						shadowStageData.m_shadowCamParamBlock.GetBuffer()->Commit(shadowCamData.m_cameraParams);
					}
				}
			};
		if (renderData.HasObjectData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> const& directionalIDs = 
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataDirectional>();

			auto directionalItr = renderData.IDBegin(directionalIDs);
			auto const& directionalItrEnd = renderData.IDEnd(directionalIDs);
			while (directionalItr != directionalItrEnd)
			{
				const bool hasShadow = directionalItr.Get<gr::Light::RenderDataDirectional>().m_hasShadow;
				Update2DShadowCamData(gr::Light::Directional, directionalItr, m_directionalShadowStageData, hasShadow);
				++directionalItr;
			}
		}
		if (renderData.HasObjectData<gr::Light::RenderDataSpot>())
		{
			std::vector<gr::RenderDataID> const& spotIDs = 
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataSpot>();

			auto spotItr = renderData.IDBegin(spotIDs);
			auto const& spotItrEnd = renderData.IDEnd(spotIDs);
			while (spotItr != spotItrEnd)
			{
				const bool hasShadow = spotItr.Get<gr::Light::RenderDataSpot>().m_hasShadow;
				Update2DShadowCamData(gr::Light::Spot, spotItr, m_spotShadowStageData, hasShadow);
				++spotItr;
			}
		}

		// Update point shadow param blocks, if necessary: 
		if (renderData.HasObjectData<gr::Light::RenderDataPoint>())
		{
			std::vector<gr::RenderDataID> const& pointIDs =
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataPoint>();
			auto pointItr = renderData.IDBegin(pointIDs);
			auto const& pointItrEnd = renderData.IDEnd(pointIDs);
			while (pointItr != pointItrEnd)
			{
				const bool hasShadow = pointItr.Get<gr::Light::RenderDataPoint>().m_hasShadow;

				SEAssert(hasShadow == false ||
					(pointItr.HasObjectData<gr::Camera::RenderData>() &&
						pointItr.HasObjectData<gr::ShadowMap::RenderData>()),
					"If a light has a shadow, it must have a shadow camera");

				if (hasShadow)
				{
					const gr::RenderDataID pointLightID = pointItr.GetRenderDataID();

					ShadowStageData& pointShadowStageData = m_pointShadowStageData.at(pointLightID);

					if (pointItr.IsDirty<gr::Camera::RenderData>() || pointItr.TransformIsDirty())
					{
						gr::Camera::RenderData const& shadowCamData = pointItr.Get<gr::Camera::RenderData>();
						gr::Transform::RenderData const& transformData = pointItr.GetTransformData();

						CubemapShadowRenderData const& cubemapShadowParams =
							GetCubemapShadowRenderParamsData(shadowCamData, transformData);

						m_pointShadowStageData.at(pointItr.GetRenderDataID()).m_shadowCamParamBlock.GetBuffer()->Commit(
							cubemapShadowParams);
					}
				}
				++pointItr;
			}
		}

		// Update the stage depth target and append permanent render stages each frame to allow dynamic light
		// creation/destruction, and in case the shadow texture buffer was reallocated
		for (auto& directionalStageItr : m_directionalShadowStageData)
		{
			const gr::RenderDataID lightID = directionalStageItr.first;

			directionalStageItr.second.m_renderStage->GetTextureTargetSet()->ReplaceDepthStencilTargetTexture(
				*m_directionalShadowArrayTex,
				CreateShadowWriteView(
					gr::Light::Directional, GetShadowArrayIdx(m_directionalShadowArrayIdxMap, lightID)));
			
			m_stagePipeline->AppendRenderStageForSingleFrame(
				m_directionalParentStageItr, directionalStageItr.second.m_renderStage);
		}
		for (auto& pointStageItr : m_pointShadowStageData)
		{
			const gr::RenderDataID lightID = pointStageItr.first;

			pointStageItr.second.m_renderStage->GetTextureTargetSet()->ReplaceDepthStencilTargetTexture(
				*m_pointShadowArrayTex,
				CreateShadowWriteView(gr::Light::Point, GetShadowArrayIdx(m_pointShadowArrayIdxMap, lightID)));

			m_stagePipeline->AppendRenderStageForSingleFrame(
				m_pointParentStageItr, pointStageItr.second.m_renderStage);
		}
		for (auto& spotStageItr : m_spotShadowStageData)
		{
			const gr::RenderDataID lightID = spotStageItr.first;

			spotStageItr.second.m_renderStage->GetTextureTargetSet()->ReplaceDepthStencilTargetTexture(
				*m_spotShadowArrayTex,
				CreateShadowWriteView(gr::Light::Spot, GetShadowArrayIdx(m_spotShadowArrayIdxMap, lightID)));

			m_stagePipeline->AppendRenderStageForSingleFrame(
				m_spotParentStageItr, spotStageItr.second.m_renderStage);
		}

		CreateBatches();
	}


	void ShadowsGraphicsSystem::CreateBatches()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		
		if (renderData.HasObjectData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> directionalIDs =
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataDirectional>();

			auto directionalItr = renderData.IDBegin(directionalIDs);
			auto const& directionalItrEnd = renderData.IDEnd(directionalIDs);
			while (directionalItr != directionalItrEnd)
			{
				gr::Light::RenderDataDirectional const& directionalData = 
					directionalItr.Get<gr::Light::RenderDataDirectional>();
				if (directionalData.m_hasShadow && directionalData.m_canContribute)
				{
					const gr::RenderDataID lightID = directionalData.m_renderDataID;

					re::RenderStage& directionalStage = 
						*m_directionalShadowStageData.at(lightID).m_renderStage;

					if (m_viewBatches)
					{
						SEAssert(m_viewBatches->contains(lightID), "Cannot find light camera ID in view batches");
						directionalStage.AddBatches(m_viewBatches->at(lightID));
					}
					else
					{
						SEAssert(m_allBatches, "Must have all batches if view batches is null");
						directionalStage.AddBatches(*m_allBatches);
					}
					
				}
				++directionalItr;
			}
		}

		if (renderData.HasObjectData<gr::Light::RenderDataSpot>())
		{
			auto AddSpotLightBatches = [&](auto& spotItr, auto const& spotItrEnd)
				{
					while (spotItr != spotItrEnd)
					{
						gr::Light::RenderDataSpot const& spotData = spotItr.Get<gr::Light::RenderDataSpot>();
						if (spotData.m_hasShadow && spotData.m_canContribute)
						{
							const gr::RenderDataID lightID = spotData.m_renderDataID;

							re::RenderStage& spotStage = *m_spotShadowStageData.at(lightID).m_renderStage;

							if (m_viewBatches)
							{
								SEAssert(m_viewBatches->contains(lightID), "Cannot find light camera ID in view batches");
								spotStage.AddBatches(m_viewBatches->at(lightID));
							}
							else
							{
								SEAssert(m_allBatches, "Must have all batches if view batches is null");
								spotStage.AddBatches(*m_allBatches);
							}
						}
						++spotItr;
					}
				};

			if (m_spotCullingResults)
			{
				auto spotItr = renderData.IDBegin(*m_spotCullingResults);
				auto const& spotItrEnd = renderData.IDEnd(*m_spotCullingResults);
				AddSpotLightBatches(spotItr, spotItrEnd);
			}
			else
			{
				auto spotItr = renderData.LinearBegin<gr::Light::RenderDataSpot>();
				auto const& spotItrEnd = renderData.LinearEnd<gr::Light::RenderDataSpot>();
				AddSpotLightBatches(spotItr, spotItrEnd);
			}
		}

		if (renderData.HasObjectData<gr::Light::RenderDataPoint>())
		{
			auto AddPointLightBatches = [&](auto& pointItr, auto const& pointItrEnd)
				{
					while (pointItr != pointItrEnd)
					{
						gr::Light::RenderDataPoint const& pointData = pointItr.Get<gr::Light::RenderDataPoint>();
						if (pointData.m_hasShadow && pointData.m_canContribute)
						{
							const gr::RenderDataID lightID = pointData.m_renderDataID;

							if (m_viewBatches)
							{
								// TODO: We're currently using a geometry shader to project shadows to cubemap faces, so
								// we need to add all batches to the same stage. This is wasteful, as 5/6 of the faces
								// don't need a given batch. We should draw each face of the cubemap seperately instead
								SEAssert(m_viewBatches->contains(lightID), "Cannot find light camera ID in view batches");
								
								std::unordered_set<util::DataHash> seenBatches;
								for (uint8_t faceIdx = 0; faceIdx < 6; ++faceIdx)
								{
									const gr::Camera::View faceView(
										pointData.m_renderDataID, static_cast<gr::Camera::View::Face>(faceIdx));

									for (auto const& batch : m_viewBatches->at(faceView))
									{
										// Different views may contain the same batch, so we only add unique ones
										const util::DataHash batchDataHash = batch.GetDataHash();
										if (!seenBatches.contains(batchDataHash))
										{
											m_pointShadowStageData.at(
												pointData.m_renderDataID).m_renderStage->AddBatch(batch);
											seenBatches.emplace(batchDataHash);
										}
									}
								}
							}
							else
							{
								SEAssert(m_allBatches, "Must have all batches if view batches is null");
								m_pointShadowStageData.at(pointData.m_renderDataID).m_renderStage->AddBatches(
									*m_allBatches);
							}
						}
						++pointItr;
					}
				};

			if (m_pointCullingResults)
			{
				auto pointItr = renderData.IDBegin(*m_pointCullingResults);
				auto const& pointItrEnd = renderData.IDEnd(*m_pointCullingResults);
				AddPointLightBatches(pointItr, pointItrEnd);
			}
			else
			{
				auto pointItr = renderData.LinearBegin<gr::Light::RenderDataPoint>();
				auto const& pointItrEnd = renderData.LinearEnd<gr::Light::RenderDataPoint>();
				AddPointLightBatches(pointItr, pointItrEnd);

			}
		}
	}
}