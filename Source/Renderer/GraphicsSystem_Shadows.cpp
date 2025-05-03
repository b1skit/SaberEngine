// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystemManager.h"
#include "LightRenderData.h"
#include "RenderDataManager.h"
#include "ShadowMapRenderData.h"

#include "Core/InvPtr.h"

#include "Shaders/Common/ShadowParams.h"


namespace
{
	CubemapShadowData GetCubemapShadowData(
		gr::Camera::RenderData const& shadowCamData, gr::Transform::RenderData const& transformData)
	{
		SEAssert(shadowCamData.m_cameraConfig.m_projectionType == gr::Camera::Config::ProjectionType::PerspectiveCubemap,
			"Invalid projection type");

		CubemapShadowData cubemapShadowParams{};
		
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

		std::shared_ptr<re::Stage> shadowStage =
			re::Stage::CreateGraphicsStage(stageName.c_str(), re::Stage::GraphicsStageParams{});

		shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::ShadowCaster, re::Stage::FilterMode::Require, true);
		shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::AlphaBlended, re::Stage::FilterMode::Exclude, true);

		shadowStage->SetDrawStyle(effect::drawstyle::Shadow_Cube);
		
		SEAssert(shadowData.m_lightType == gr::Light::Point, "Unexpected light type for a cube stage");

		// Texture target set:
		std::shared_ptr<re::TextureTargetSet> pointShadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_CubeShadowTargetSet", lightName));
		
		SEAssert(GetShadowArrayIdx(m_pointShadowArrayIdxMap, lightID) < (*m_pointShadowArrayTex)->GetTextureParams().m_arraySize,
			"Shadow array index is out of bounds");

		pointShadowTargetSet->SetDepthStencilTarget(
			*m_pointShadowArrayTex,
			re::TextureTarget::TargetParams{
				.m_textureView = CreateShadowWriteView(
					shadowData.m_lightType, 
					GetShadowArrayIdx(m_pointShadowArrayIdxMap, lightID)) });

		pointShadowTargetSet->SetViewport(*m_pointShadowArrayTex);
		pointShadowTargetSet->SetScissorRect(*m_pointShadowArrayTex);

		shadowStage->SetTextureTargetSet(pointShadowTargetSet);

		// Cubemap shadow buffer:
		CubemapShadowData const& cubemapShadowParams = GetCubemapShadowData(camData, transformData);

		re::BufferInput cubeShadowBuf(
			CubemapShadowData::s_shaderName,
			re::Buffer::Create(
				CubemapShadowData::s_shaderName,
				cubemapShadowParams,
				re::Buffer::BufferParams{
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Constant,
				}));

		shadowStage->AddPermanentBuffer(cubeShadowBuf);
		
		std::shared_ptr<re::ClearTargetSetStage> shadowClearStage =
			re::Stage::CreateTargetSetClearStage("Shadows: Cube shadow clear stage", pointShadowTargetSet);
		shadowClearStage->EnableDepthClear(1.f);

		dstStageData.emplace(
			lightID,
			ShadowStageData{
				.m_clearStage = shadowClearStage,
				.m_stage = shadowStage,
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

		std::shared_ptr<re::Stage> shadowStage =
			re::Stage::CreateGraphicsStage(stageName.c_str(), re::Stage::GraphicsStageParams{});

		shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::ShadowCaster, re::Stage::FilterMode::Require, true);

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
		
		SEAssert(GetShadowArrayIdx(shadowArrayIdxMap, lightID) < shadowArrayTex->GetTextureParams().m_arraySize,
			"Shadow array index is out of bounds");

		shadowTargetSet->SetDepthStencilTarget(
			shadowArrayTex, 
			re::TextureTarget::TargetParams{
				.m_textureView = CreateShadowWriteView(
					shadowData.m_lightType,
					GetShadowArrayIdx(shadowArrayIdxMap, lightID)) });

		shadowTargetSet->SetViewport(shadowArrayTex);
		shadowTargetSet->SetScissorRect(shadowArrayTex);

		shadowStage->SetTextureTargetSet(shadowTargetSet);

		std::shared_ptr<re::ClearTargetSetStage> shadowClearStage =
			re::Stage::CreateTargetSetClearStage("Shadows: 2D shadow clear stage", shadowTargetSet);
		shadowClearStage->EnableDepthClear(1.f);

		dstStageData.emplace(
			lightID,
			ShadowStageData{
				.m_clearStage = shadowClearStage,
				.m_stage = shadowStage,
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

		std::shared_ptr<re::Stage> directionalParentStage = 
			re::Stage::CreateParentStage("Directional shadow stages");
		m_directionalParentStageItr = pipeline.AppendStage(directionalParentStage);

		std::shared_ptr<re::Stage> pointParentStage = re::Stage::CreateParentStage("Point shadow stages");
		m_pointParentStageItr = pipeline.AppendStage(pointParentStage);

		std::shared_ptr<re::Stage> spotParentStage = re::Stage::CreateParentStage("Spot shadow stages");
		m_spotParentStageItr = pipeline.AppendStage(spotParentStage);

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

			for (auto const& lightItr : gr::IDAdapter(renderData, *newLightIDs))
			{
				if (lightItr->HasObjectData<gr::ShadowMap::RenderData>())
				{
					SEAssert(lightItr->HasObjectData<gr::Camera::RenderData>(),
						"Shadow map and shadow camera render data are both required for shadows");

					CreateRegister2DShadowStage(
						dstStageData,
						lightItr->GetRenderDataID(),
						lightItr->Get<gr::ShadowMap::RenderData>(),
						lightItr->Get<gr::Camera::RenderData>());
				}
			}
		};
		Register2DShadowLights(renderData.GetIDsWithNewData<gr::Light::RenderDataDirectional>(), m_directionalShadowStageData);
		Register2DShadowLights(renderData.GetIDsWithNewData<gr::Light::RenderDataSpot>(), m_spotShadowStageData);

		// Register new point lights:
		std::vector<gr::RenderDataID> const* newPointIDs =
			renderData.GetIDsWithNewData<gr::Light::RenderDataPoint>();
		if (newPointIDs)
		{
			for (auto const& pointItr : gr::IDAdapter(renderData, *newPointIDs))
			{
				if (pointItr->HasObjectData<gr::ShadowMap::RenderData>())
				{
					SEAssert(pointItr->HasObjectData<gr::Camera::RenderData>(),
						"Shadow map and shadow camera render data are both required for shadows");

					CreateRegisterCubeShadowStage(
						m_pointShadowStageData,
						pointItr->GetRenderDataID(),
						pointItr->Get<gr::ShadowMap::RenderData>(),
						pointItr->GetTransformData(),
						pointItr->Get<gr::Camera::RenderData>());
				}
			}
		}


		// Update directional and spot shadow buffers, if necessary:
		auto Update2DShadowCamData = [](
			gr::Light::Type lightType,
			auto&& lightItr,
			std::unordered_map<gr::RenderDataID, ShadowStageData>& stageData,
			bool hasShadow)
			{
				SEAssert(hasShadow == false ||
					(lightItr->HasObjectData<gr::Camera::RenderData>() &&
						lightItr->HasObjectData<gr::ShadowMap::RenderData>()),
					"If a light has a shadow, it must have a shadow camera");

				if (hasShadow)
				{
					const gr::RenderDataID lightID = lightItr->GetRenderDataID();

					ShadowStageData& shadowStageData = stageData.at(lightID);

					if (lightItr->IsDirty<gr::Camera::RenderData>() || lightItr->TransformIsDirty())
					{
						gr::Camera::RenderData const& shadowCamData = lightItr->Get<gr::Camera::RenderData>();

						shadowStageData.m_shadowCamParamBlock.GetBuffer()->Commit(shadowCamData.m_cameraParams);
					}
				}
			};
		if (renderData.HasObjectData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> const& directionalIDs = 
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataDirectional>();

			for (auto const& directionalItr : gr::IDAdapter(renderData, directionalIDs))
			{
				const bool hasShadow = directionalItr->Get<gr::Light::RenderDataDirectional>().m_hasShadow;
				Update2DShadowCamData(gr::Light::Directional, directionalItr, m_directionalShadowStageData, hasShadow);
			}
		}
		if (renderData.HasObjectData<gr::Light::RenderDataSpot>())
		{
			std::vector<gr::RenderDataID> const& spotIDs = 
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataSpot>();

			for (auto const& spotItr : gr::IDAdapter(renderData, spotIDs))
			{
				const bool hasShadow = spotItr->Get<gr::Light::RenderDataSpot>().m_hasShadow;
				Update2DShadowCamData(gr::Light::Spot, spotItr, m_spotShadowStageData, hasShadow);
			}
		}

		// Update point shadow param blocks, if necessary: 
		if (renderData.HasObjectData<gr::Light::RenderDataPoint>())
		{
			std::vector<gr::RenderDataID> const& pointIDs =
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataPoint>();

			for (auto const& pointItr : gr::IDAdapter(renderData, pointIDs))
			{
				const bool hasShadow = pointItr->Get<gr::Light::RenderDataPoint>().m_hasShadow;

				SEAssert(hasShadow == false ||
					(pointItr->HasObjectData<gr::Camera::RenderData>() &&
						pointItr->HasObjectData<gr::ShadowMap::RenderData>()),
					"If a light has a shadow, it must have a shadow camera");

				if (hasShadow)
				{
					const gr::RenderDataID pointLightID = pointItr->GetRenderDataID();

					ShadowStageData& pointShadowStageData = m_pointShadowStageData.at(pointLightID);

					if (pointItr->IsDirty<gr::Camera::RenderData>() || pointItr->TransformIsDirty())
					{
						gr::Camera::RenderData const& shadowCamData = pointItr->Get<gr::Camera::RenderData>();
						gr::Transform::RenderData const& transformData = pointItr->GetTransformData();

						CubemapShadowData const& cubemapShadowParams =
							GetCubemapShadowData(shadowCamData, transformData);

						m_pointShadowStageData.at(pointItr->GetRenderDataID()).m_shadowCamParamBlock.GetBuffer()->Commit(
							cubemapShadowParams);
					}
				}
			}
		}

		// Update the stage depth target and append permanent render stages each frame to allow dynamic light
		// creation/destruction, and in case the shadow texture buffer was reallocated
		for (auto& directionalStageItr : m_directionalShadowStageData)
		{
			const gr::RenderDataID lightID = directionalStageItr.first;

			directionalStageItr.second.m_stage->GetTextureTargetSet()->ReplaceDepthStencilTargetTexture(
				*m_directionalShadowArrayTex,
				CreateShadowWriteView(
					gr::Light::Directional, GetShadowArrayIdx(m_directionalShadowArrayIdxMap, lightID)));
			
			auto clearItr = m_stagePipeline->AppendStageForSingleFrame(
				m_directionalParentStageItr, directionalStageItr.second.m_clearStage);

			m_stagePipeline->AppendStageForSingleFrame(
				clearItr, directionalStageItr.second.m_stage);
		}
		for (auto& pointStageItr : m_pointShadowStageData)
		{
			const gr::RenderDataID lightID = pointStageItr.first;

			pointStageItr.second.m_stage->GetTextureTargetSet()->ReplaceDepthStencilTargetTexture(
				*m_pointShadowArrayTex,
				CreateShadowWriteView(gr::Light::Point, GetShadowArrayIdx(m_pointShadowArrayIdxMap, lightID)));

			auto clearItr = m_stagePipeline->AppendStageForSingleFrame(
				m_pointParentStageItr, pointStageItr.second.m_clearStage);

			m_stagePipeline->AppendStageForSingleFrame(
				clearItr, pointStageItr.second.m_stage);
		}
		for (auto& spotStageItr : m_spotShadowStageData)
		{
			const gr::RenderDataID lightID = spotStageItr.first;

			spotStageItr.second.m_stage->GetTextureTargetSet()->ReplaceDepthStencilTargetTexture(
				*m_spotShadowArrayTex,
				CreateShadowWriteView(gr::Light::Spot, GetShadowArrayIdx(m_spotShadowArrayIdxMap, lightID)));

			auto clearItr = m_stagePipeline->AppendStageForSingleFrame(
				m_spotParentStageItr, spotStageItr.second.m_clearStage);

			m_stagePipeline->AppendStageForSingleFrame(
				clearItr, spotStageItr.second.m_stage);
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

			for (auto const& directionalItr : gr::IDAdapter(renderData, directionalIDs))
			{
				gr::Light::RenderDataDirectional const& directionalData = 
					directionalItr->Get<gr::Light::RenderDataDirectional>();
				if (directionalData.m_hasShadow && directionalData.m_canContribute)
				{
					const gr::RenderDataID lightID = directionalData.m_renderDataID;

					re::Stage& directionalStage = 
						*m_directionalShadowStageData.at(lightID).m_stage;

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
			}
		}

		if (renderData.HasObjectData<gr::Light::RenderDataSpot>())
		{
			auto AddSpotLightBatches = [&](auto const& spotObjects)
				{
					for (auto const& spotItr : spotObjects)
					{
						gr::Light::RenderDataSpot const& spotData = spotItr->Get<gr::Light::RenderDataSpot>();
						if (spotData.m_hasShadow && spotData.m_canContribute)
						{
							const gr::RenderDataID lightID = spotData.m_renderDataID;

							re::Stage& spotStage = *m_spotShadowStageData.at(lightID).m_stage;

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
					}
				};

			if (m_spotCullingResults)
			{
				AddSpotLightBatches(gr::IDAdapter(renderData, *m_spotCullingResults));
			}
			else
			{
				AddSpotLightBatches(gr::LinearAdapter<gr::Light::RenderDataSpot>(renderData));
			}
		}

		if (renderData.HasObjectData<gr::Light::RenderDataPoint>())
		{
			auto AddPointLightBatches = [&](auto&& pointObjects)
				{
					for (auto const& pointItr : pointObjects)
					{
						gr::Light::RenderDataPoint const& pointData = pointItr->Get<gr::Light::RenderDataPoint>();
						if (pointData.m_hasShadow && pointData.m_canContribute)
						{
							const gr::RenderDataID lightID = pointData.m_renderDataID;

							if (m_viewBatches)
							{
								// TODO: We're currently using a geometry shader to project shadows to cubemap faces, so
								// we need to add all batches to the same stage. This is wasteful, as 5/6 of the faces
								// don't need a given batch. We should draw each face of the cubemap seperately instead
								SEAssert(m_viewBatches->contains(lightID), "Cannot find light camera ID in view batches");
								
								std::unordered_set<util::HashKey> seenBatches;
								for (uint8_t faceIdx = 0; faceIdx < 6; ++faceIdx)
								{
									const gr::Camera::View faceView(
										pointData.m_renderDataID, static_cast<gr::Camera::View::Face>(faceIdx));

									for (auto const& batch : m_viewBatches->at(faceView))
									{
										// Different views may contain the same batch, so we only add unique ones
										const util::HashKey batchDataHash = batch.GetDataHash();
										if (!seenBatches.contains(batchDataHash))
										{
											m_pointShadowStageData.at(
												pointData.m_renderDataID).m_stage->AddBatch(batch);
											seenBatches.emplace(batchDataHash);
										}
									}
								}
							}
							else
							{
								SEAssert(m_allBatches, "Must have all batches if view batches is null");
								m_pointShadowStageData.at(pointData.m_renderDataID).m_stage->AddBatches(
									*m_allBatches);
							}
						}
					}
				};

			if (m_pointCullingResults)
			{
				AddPointLightBatches(gr::IDAdapter(renderData, *m_pointCullingResults));
			}
			else
			{
				AddPointLightBatches(gr::LinearAdapter<gr::Light::RenderDataPoint>(renderData));
			}
		}
	}
}