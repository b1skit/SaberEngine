// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "GraphicsEvent.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystemManager.h"
#include "LightParamsHelpers.h"
#include "LightRenderData.h"
#include "RenderDataManager.h"
#include "ShadowMapRenderData.h"
#include "Texture.h"
#include "TransformRenderData.h"

#include "Core/Config.h"
#include "Core/InvPtr.h"

#include "Renderer/Shaders/Common/LightParams.h"
#include "Renderer/Shaders/Common/ShadowParams.h"


namespace
{
	CubeShadowRenderData CreateCubemapShadowData(
		gr::Camera::RenderData const& shadowCamData, gr::Transform::RenderData const& lightTransformData)
	{
		SEAssert(shadowCamData.m_cameraConfig.m_projectionType == gr::Camera::Config::ProjectionType::PerspectiveCubemap,
			"Invalid projection type");

		CubeShadowRenderData cubemapShadowParams{};

		std::vector<glm::mat4> const& cubeViewMatrices =
			gr::Camera::BuildAxisAlignedCubeViewMatrices(lightTransformData.m_globalPosition);

		for (uint8_t faceIdx = 0; faceIdx < 6; faceIdx++)
		{
			cubemapShadowParams.g_cubemapShadowCam_VP[faceIdx] =
				shadowCamData.m_cameraParams.g_projection * cubeViewMatrices[faceIdx];
		}

		cubemapShadowParams.g_cubemapShadowCamNearFar =
			glm::vec4(shadowCamData.m_cameraConfig.m_near, shadowCamData.m_cameraConfig.m_far, 0.f, 0.f);

		cubemapShadowParams.g_cubemapLightWorldPos = glm::vec4(lightTransformData.m_globalPosition, 0.f);

		return cubemapShadowParams;
	}


	re::TextureView CreateShadowWriteView(gr::Light::Type lightType, uint32_t shadowTexArrayIdx)
	{
		switch (lightType)
		{
		case gr::Light::Directional: return re::TextureView(re::TextureView::Texture2DArrayView{ 0, 1, shadowTexArrayIdx, 1 });
		case gr::Light::Point: return re::TextureView(re::TextureView::Texture2DArrayView{ 0, 1, shadowTexArrayIdx * 6, 6 });
		case gr::Light::Spot: return re::TextureView(re::TextureView::Texture2DArrayView{ 0, 1, shadowTexArrayIdx, 1 });
		case gr::Light::AmbientIBL:
		default: SEAssertF("Invalid light type");
		}
		return re::TextureView(re::TextureView::Texture2DArrayView{ 0, 1, shadowTexArrayIdx, 1 }); // This should never happen
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
	{
	}


	void ShadowsGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_pointLightCullingDataInput);
		RegisterDataInput(k_spotLightCullingDataInput);

		RegisterDataInput(k_viewBatchesDataInput);
		RegisterDataInput(k_allBatchesDataInput);

		RegisterDataInput(k_lightIDToShadowRecordInput);
	}


	void ShadowsGraphicsSystem::RegisterOutputs()
	{
		// Shadow array textures:
		RegisterTextureOutput(k_directionalShadowArrayTexOutput, &m_directionalShadowTexMetadata.m_shadowArray);
		RegisterTextureOutput(k_pointShadowArrayTexOutput, &m_pointShadowTexMetadata.m_shadowArray);
		RegisterTextureOutput(k_spotShadowArrayTexOutput, &m_spotShadowTexMetadata.m_shadowArray);

		RegisterDataOutput(k_lightIDToShadowRecordOutput, &m_lightIDToShadowRecords);

		RegisterBufferOutput(k_PCSSSampleParamsBufferOutput, &m_poissonSampleParamsBuffer);
	}


	void ShadowsGraphicsSystem::CreateRegisterCubeShadowStage(
		std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
		gr::RenderDataID lightID,
		gr::Light::Type lightType,
		gr::ShadowMap::RenderData const& shadowData,
		gr::Transform::RenderData const& transformData,
		gr::Camera::RenderData const& camData)
	{
		char const* lightName = shadowData.m_owningLightName;
		std::string const& stageName = std::format("{}_CubeShadow", lightName);

		std::shared_ptr<gr::Stage> shadowStage =
			gr::Stage::CreateGraphicsStage(stageName.c_str(), gr::Stage::GraphicsStageParams{});

		shadowStage->SetBatchFilterMaskBit(gr::Batch::Filter::ShadowCaster, gr::Stage::FilterMode::Require, true);
		shadowStage->SetBatchFilterMaskBit(gr::Batch::Filter::AlphaBlended, gr::Stage::FilterMode::Exclude, true);

		shadowStage->AddDrawStyleBits(effect::drawstyle::Shadow_Cube);
		
		SEAssert(shadowData.m_lightType == gr::Light::Point, "Unexpected light type for a cube stage");

		// Texture target set:
		std::shared_ptr<re::TextureTargetSet> pointShadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_CubeShadowTargetSet", lightName));
		
		SEAssert(m_lightIDToShadowRecords.contains(lightID), "Failed to find a shadow record");
		gr::ShadowRecord const& shadowRecord = m_lightIDToShadowRecords.at(lightID);

		SEAssert(shadowRecord.m_shadowTexArrayIdx < (*shadowRecord.m_shadowTex)->GetTextureParams().m_arraySize,
			"Shadow array index is out of bounds");

		pointShadowTargetSet->SetDepthStencilTarget(
			*shadowRecord.m_shadowTex,
			re::TextureTarget::TargetParams{
				.m_textureView = CreateShadowWriteView(
					shadowData.m_lightType, 
					shadowRecord.m_shadowTexArrayIdx) });

		pointShadowTargetSet->SetViewport(*shadowRecord.m_shadowTex);
		pointShadowTargetSet->SetScissorRect(*shadowRecord.m_shadowTex);

		shadowStage->SetTextureTargetSet(pointShadowTargetSet);

		// Cubemap shadow buffer:
		CubeShadowRenderData const& cubemapShadowParams = CreateCubemapShadowData(camData, transformData);

		re::BufferInput cubeShadowBuf(
			CubeShadowRenderData::s_shaderName,
			re::Buffer::Create(
				CubeShadowRenderData::s_shaderName,
				cubemapShadowParams,
				re::Buffer::BufferParams{
					.m_stagingPool = re::Buffer::StagingPool::Permanent,
					.m_memPoolPreference = re::Buffer::UploadHeap,
					.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
					.m_usageMask = re::Buffer::Constant,
				}));

		shadowStage->AddPermanentBuffer(cubeShadowBuf);
		
		std::shared_ptr<gr::ClearTargetSetStage> shadowClearStage =
			gr::Stage::CreateTargetSetClearStage("Shadows: Cube shadow clear stage", pointShadowTargetSet);
		shadowClearStage->EnableDepthClear(1.f);

		dstStageData.emplace(
			lightID,
			ShadowStageData{
				.m_clearStage = shadowClearStage,
				.m_stage = shadowStage,
				.m_shadowTargetSet = pointShadowTargetSet,
				.m_shadowRenderCameraParams = cubeShadowBuf,
				.m_lightType = lightType, });
	}


	void ShadowsGraphicsSystem::CreateRegister2DShadowStage(
		std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
		gr::RenderDataID lightID,
		gr::Light::Type lightType,
		gr::ShadowMap::RenderData const& shadowData,
		gr::Camera::RenderData const& shadowCamData)
	{
		char const* lightName = shadowData.m_owningLightName;
		std::string const& stageName = std::format("{}_2DShadow", lightName);

		std::shared_ptr<gr::Stage> shadowStage =
			gr::Stage::CreateGraphicsStage(stageName.c_str(), gr::Stage::GraphicsStageParams{});

		shadowStage->SetBatchFilterMaskBit(gr::Batch::Filter::ShadowCaster, gr::Stage::FilterMode::Require, true);

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

		shadowStage->AddDrawStyleBits(effect::drawstyle::Shadow_2D);

		SEAssert(m_lightIDToShadowRecords.contains(lightID), "Failed to find a shadow record");
		gr::ShadowRecord const& shadowRecord = m_lightIDToShadowRecords.at(lightID);

		// Texture target set:
		std::shared_ptr<re::TextureTargetSet> shadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_2DShadowTargetSet", lightName));
		
		SEAssert(shadowRecord.m_shadowTexArrayIdx < (*shadowRecord.m_shadowTex)->GetTextureParams().m_arraySize,
			"Shadow array index is out of bounds");

		shadowTargetSet->SetDepthStencilTarget(
			*shadowRecord.m_shadowTex,
			re::TextureTarget::TargetParams{
				.m_textureView = CreateShadowWriteView(
					shadowData.m_lightType,
					shadowRecord.m_shadowTexArrayIdx) });

		shadowTargetSet->SetViewport(*shadowRecord.m_shadowTex);
		shadowTargetSet->SetScissorRect(*shadowRecord.m_shadowTex);

		shadowStage->SetTextureTargetSet(shadowTargetSet);

		std::shared_ptr<gr::ClearTargetSetStage> shadowClearStage =
			gr::Stage::CreateTargetSetClearStage("Shadows: 2D shadow clear stage", shadowTargetSet);
		shadowClearStage->EnableDepthClear(1.f);

		dstStageData.emplace(
			lightID,
			ShadowStageData{
				.m_clearStage = shadowClearStage,
				.m_stage = shadowStage,
				.m_shadowTargetSet = shadowTargetSet,
				.m_shadowRenderCameraParams = shadowCamParams,
				.m_lightType = lightType, });
	}


	void ShadowsGraphicsSystem::InitPipeline(
		gr::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const&,
		DataDependencies const& dataDependencies)
	{
		m_stagePipeline = &pipeline;

		std::shared_ptr<gr::Stage> directionalParentStage = 
			gr::Stage::CreateParentStage("Directional shadow stages");
		m_directionalParentStageItr = pipeline.AppendStage(directionalParentStage);

		std::shared_ptr<gr::Stage> pointParentStage = gr::Stage::CreateParentStage("Point shadow stages");
		m_pointParentStageItr = pipeline.AppendStage(pointParentStage);

		std::shared_ptr<gr::Stage> spotParentStage = gr::Stage::CreateParentStage("Spot shadow stages");
		m_spotParentStageItr = pipeline.AppendStage(spotParentStage);

		// Cache our dependencies:
		m_pointCullingResults = GetDependency<PunctualLightCullingResults>(k_pointLightCullingDataInput, dataDependencies, false);
		m_spotCullingResults = GetDependency<PunctualLightCullingResults>(k_spotLightCullingDataInput, dataDependencies, false);

		m_viewBatches = GetDependency<ViewBatches>(k_viewBatchesDataInput, dataDependencies, false);
		m_allBatches = GetDependency<AllBatches>(k_allBatchesDataInput, dataDependencies, false);
		SEAssert(m_viewBatches || m_allBatches, "Must have received some batches");

		// PCSS sample buffer::
		m_poissonSampleParamsBuffer = re::Buffer::Create(
			PoissonSampleParamsData::s_shaderName,
			grutil::GetPoissonSampleParamsData(),
			re::Buffer::BufferParams{
				.m_stagingPool = re::Buffer::StagingPool::Temporary,
				.m_memPoolPreference = re::Buffer::UploadHeap,
				.m_accessMask = re::Buffer::GPURead | re::Buffer::CPUWrite,
				.m_usageMask = re::Buffer::Constant,
			});
	}


	void ShadowsGraphicsSystem::RegisterNewShadowStages()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		std::vector<gr::RenderDataID> const* newShadowIDs = renderData.GetIDsWithNewData<gr::ShadowMap::RenderData>();

		if (!newShadowIDs)
		{
			return;
		}

		for (auto const& itr : gr::IDAdapter(renderData, *newShadowIDs))
		{
			SEAssert(itr->HasObjectData<gr::ShadowMap::RenderData>(),
				"No ShadowMap RenderData found. This should not be possible");

			SEAssert(itr->HasObjectData<gr::Camera::RenderData>(),
				"Shadow map and shadow camera render data are both required for shadows");

			gr::ShadowMap::RenderData const& shadowData = itr->Get<gr::ShadowMap::RenderData>();

			switch (shadowData.m_lightType)
			{
			case gr::Light::Directional:
			case gr::Light::Spot:
			{
				CreateRegister2DShadowStage(
					m_shadowStageData,
					itr->GetRenderDataID(),
					shadowData.m_lightType,
					shadowData,
					itr->Get<gr::Camera::RenderData>());
			}
			break;
			case gr::Light::Point:
			{
				CreateRegisterCubeShadowStage(
					m_shadowStageData,
					itr->GetRenderDataID(),
					shadowData.m_lightType,
					shadowData,
					itr->GetTransformData(),
					itr->Get<gr::Camera::RenderData>());
			}
			break;
			case gr::Light::AmbientIBL:
			default: SEAssertF("Invalid light type");
			}
		}
	}


	void ShadowsGraphicsSystem::UpdateShadowStages()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Update dirty shadow buffer data:
		std::vector<gr::RenderDataID> const& dirtyShadows = 
			renderData.GetIDsWithAnyDirtyData<gr::ShadowMap::RenderData, gr::Camera::RenderData, gr::Transform::RenderData>();

		for (auto const& itr : gr::IDAdapter(renderData, dirtyShadows))
		{
			SEAssert((itr->HasObjectData<gr::Camera::RenderData>() &&
				itr->HasObjectData<gr::ShadowMap::RenderData>()),
				"If a light has a shadow, it must have a shadow camera");

			const gr::RenderDataID lightID = itr->GetRenderDataID();

			ShadowStageData& shadowStageData = m_shadowStageData.at(lightID);

			if (itr->IsDirty<gr::Camera::RenderData>() || itr->TransformIsDirty())
			{
				gr::ShadowMap::RenderData const& shadowData = itr->Get<gr::ShadowMap::RenderData>();
				gr::Camera::RenderData const& shadowCamData = itr->Get<gr::Camera::RenderData>();

				switch (shadowData.m_lightType)
				{
				case gr::Light::Directional:
				case gr::Light::Spot:
				{
					shadowStageData.m_shadowRenderCameraParams.GetBuffer()->Commit(shadowCamData.m_cameraParams);
				}
				break;
				case gr::Light::Point:
				{
					gr::Transform::RenderData const& transformData = itr->GetTransformData();

					CubeShadowRenderData const& cubemapShadowParams =
						CreateCubemapShadowData(shadowCamData, transformData);

					shadowStageData.m_shadowRenderCameraParams.GetBuffer()->Commit(cubemapShadowParams);
				}
				break;
				case gr::Light::AmbientIBL:
				default: SEAssertF("Invalid light type");
				}
			}
		}

		// Update the stage depth target and append permanent render stages each frame to allow dynamic light
		// creation/destruction, and in case the shadow texture buffer was reallocated
		for (auto& itr : m_shadowStageData)
		{
			const gr::RenderDataID lightID = itr.first;

			SEAssert(m_lightIDToShadowRecords.contains(lightID), "Failed to find a shadow record");
			gr::ShadowRecord const& shadowRecord = m_lightIDToShadowRecords.at(lightID);

			SEAssert(shadowRecord.m_shadowTexArrayIdx < (*shadowRecord.m_shadowTex)->GetTextureParams().m_arraySize,
				"Shadow array index is out of bounds");

			itr.second.m_clearStage->GetTextureTargetSet()->ReplaceDepthStencilTargetTexture(
				*shadowRecord.m_shadowTex,
				CreateShadowWriteView(itr.second.m_lightType, shadowRecord.m_shadowTexArrayIdx));

			itr.second.m_stage->GetTextureTargetSet()->ReplaceDepthStencilTargetTexture(
				*shadowRecord.m_shadowTex,
				CreateShadowWriteView(itr.second.m_lightType, shadowRecord.m_shadowTexArrayIdx));

			gr::StagePipeline::StagePipelineItr clearItr;
			switch (itr.second.m_lightType)
			{
			case gr::Light::Directional:
			{
				clearItr = m_stagePipeline->AppendStageForSingleFrame(m_directionalParentStageItr, itr.second.m_clearStage);
			}
			break;
			case gr::Light::Spot:
			{
				clearItr = m_stagePipeline->AppendStageForSingleFrame(m_spotParentStageItr, itr.second.m_clearStage);
			}
			break;
			case gr::Light::Point:
			{
				clearItr = m_stagePipeline->AppendStageForSingleFrame(m_pointParentStageItr, itr.second.m_clearStage);
			}
			break;
			case gr::Light::AmbientIBL:
			default: SEAssertF("Invalid light type");
			}

			m_stagePipeline->AppendStageForSingleFrame(clearItr, itr.second.m_stage);
		}
	}


	void ShadowsGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Shadow texture arrays:
		RemoveDeletedShadowRecords(renderData);
		RegisterNewShadowTextureElements(renderData);

		// Stages and buffers:
		RegisterNewShadowStages();
		UpdateShadowStages();

		CreateBatches();
	}


	void ShadowsGraphicsSystem::CreateBatches()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();


		auto AddBatches = [&renderData, this](std::vector<gr::RenderDataID> const* lightIDs)
			{
				if (!lightIDs || lightIDs->empty())
				{
					return;
				}

				for (auto const& lightItr : gr::IDAdapter(renderData, *lightIDs))
				{
					if (!lightItr->HasObjectData<gr::ShadowMap::RenderData>())
					{
						continue;
					}

					const gr::RenderDataID lightID = lightItr->GetRenderDataID();

					ShadowStageData& shadowStageData = m_shadowStageData.at(lightID);
					
					switch (shadowStageData.m_lightType)
					{
					case gr::Light::Directional:
					case gr::Light::Spot:
					{
						bool canContribute = false;
						if (shadowStageData.m_lightType == gr::Light::Directional)
						{
							gr::Light::RenderDataDirectional const& directionalData =
								lightItr->Get<gr::Light::RenderDataDirectional>();
							canContribute = directionalData.m_canContribute;
						}
						else
						{
							gr::Light::RenderDataSpot const& spotData = lightItr->Get<gr::Light::RenderDataSpot>();
							canContribute = spotData.m_canContribute;
						}

						if (canContribute)
						{
							if (m_viewBatches)
							{
								SEAssert(m_viewBatches->contains(lightID), "Cannot find light camera ID in view batches");
								shadowStageData.m_stage->AddBatches(m_viewBatches->at(lightID));
							}
							else
							{
								SEAssert(m_allBatches, "Must have all batches if view batches is null");
								shadowStageData.m_stage->AddBatches(*m_allBatches);
							}
						}
					}
					break;
					case gr::Light::Point:
					{
						if (m_viewBatches)
						{
							// TODO: We're currently using a geometry shader to project shadows to cubemap faces, so
							// we need to add all batches to the same stage. This is wasteful, as 5/6 of the faces
							// don't need a given batch. We should draw each face of the cubemap seperately instead
							SEAssert(m_viewBatches->contains(lightID), "Cannot find light camera ID in view batches");

							std::unordered_set<util::HashKey> seenBatches;
							for (uint8_t faceIdx = 0; faceIdx < 6; ++faceIdx)
							{
								const gr::Camera::View faceView(lightID, static_cast<gr::Camera::View::Face>(faceIdx));

								for (auto const& batch : m_viewBatches->at(faceView))
								{
									// Different views may contain the same batch, so we only add unique ones
									const util::HashKey batchDataHash = batch->GetDataHash();
									if (!seenBatches.contains(batchDataHash))
									{
										shadowStageData.m_stage->AddBatch(batch);
										seenBatches.emplace(batchDataHash);
									}
								}
							}
						}
						else
						{
							SEAssert(m_allBatches, "Must have all batches if view batches is null");
							shadowStageData.m_stage->AddBatches(*m_allBatches);
						}
					}
					break;
					case gr::Light::AmbientIBL:
					default: SEAssertF("Invalid light type");
					}
				}
			};

		AddBatches(renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataDirectional>());

		if (m_spotCullingResults)
		{
			AddBatches(m_spotCullingResults);
		}
		else
		{
			AddBatches(renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataSpot>());
		}

		if (m_pointCullingResults)
		{
			AddBatches(m_pointCullingResults);
		}
		else
		{
			AddBatches(renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataPoint>());
		}
	}


	uint32_t ShadowsGraphicsSystem::GetShadowArrayIndex(
		ShadowTextureMetadata const& shadowMetadata, gr::RenderDataID lightID) const
	{
		uint32_t shadowTexArrayIdx = INVALID_SHADOW_IDX;
		if (shadowMetadata.m_renderDataIDToTexArrayIdx.contains(lightID))
		{
			shadowTexArrayIdx = shadowMetadata.m_renderDataIDToTexArrayIdx.at(lightID);
		}
		return shadowTexArrayIdx;
	};


	uint32_t ShadowsGraphicsSystem::GetShadowArrayIndex(gr::Light::Type lightType, gr::RenderDataID lightID) const
	{
		switch (lightType)
		{
		case gr::Light::Directional:
		{
			return GetShadowArrayIndex(m_directionalShadowTexMetadata, lightID);
		}
		break;
		case gr::Light::Point:
		{
			return GetShadowArrayIndex(m_pointShadowTexMetadata, lightID);
		}
		break;
		case gr::Light::Spot:
		{
			return GetShadowArrayIndex(m_spotShadowTexMetadata, lightID);
		}
		break;
		case gr::Light::AmbientIBL:
		default: SEAssertF("Invalid light type");
		}
		return 0; // This should never happen
	}


	void ShadowsGraphicsSystem::RemoveDeletedShadowRecords(gr::RenderDataManager const& renderData)
	{
		std::vector<gr::RenderDataID> const* deletedShadows =
			renderData.GetIDsWithDeletedData<gr::ShadowMap::RenderData>();
		if (deletedShadows && !deletedShadows->empty())
		{
			for (gr::RenderDataID deletedID : *deletedShadows)
			{
				// Delete stage data:
				m_shadowStageData.erase(deletedID);

				// Delete texture data:
				bool foundShadow = false;
				auto DeleteShadowTexEntry = [&deletedID, &foundShadow](ShadowTextureMetadata& shadowMetadata)
					{
						if (!shadowMetadata.m_renderDataIDToTexArrayIdx.contains(deletedID))
						{
							return;
						}
						foundShadow = true;

						const uint32_t deletedIdx = shadowMetadata.m_renderDataIDToTexArrayIdx.at(deletedID);

						SEAssert(shadowMetadata.m_texArrayIdxToRenderDataID.contains(deletedIdx),
							"Trying to delete a light index that has not been registered");

						// Use the reverse iterator to get the details of the last entry:
						const uint32_t lastIdx = shadowMetadata.m_texArrayIdxToRenderDataID.rbegin()->first;
						const gr::RenderDataID lastLightID = shadowMetadata.m_texArrayIdxToRenderDataID.rbegin()->second;

						SEAssert(lastIdx != deletedIdx ||
							(shadowMetadata.m_texArrayIdxToRenderDataID.at(lastIdx) == deletedID &&
								shadowMetadata.m_renderDataIDToTexArrayIdx.at(deletedID) == lastIdx),
							"IDs are out of sync");

						// Move the last entry to replace the one being deleted:
						if (lastIdx != deletedIdx)
						{
							// Update the metadata: The last element is moved to the deleted location
							shadowMetadata.m_texArrayIdxToRenderDataID.at(deletedIdx) = lastLightID;
							shadowMetadata.m_renderDataIDToTexArrayIdx.at(lastLightID) = deletedIdx;
						}

						// Update the metadata: We remove the deleted/final element:
						shadowMetadata.m_texArrayIdxToRenderDataID.erase(lastIdx);
						shadowMetadata.m_renderDataIDToTexArrayIdx.erase(deletedID);

						SEAssert(shadowMetadata.m_numShadows >= 1, "Removing this light will underflow the counter");
						shadowMetadata.m_numShadows--;
					};
				// Try to delete in order of most expected lights to least:
				DeleteShadowTexEntry(m_pointShadowTexMetadata);
				if (!foundShadow)
				{
					DeleteShadowTexEntry(m_spotShadowTexMetadata);
				}
				if (!foundShadow)
				{
					DeleteShadowTexEntry(m_directionalShadowTexMetadata);
				}
				SEAssert(foundShadow, "Trying to delete a light RenderDataID that has not been registered");

				// Update the shadow record output:
				SEAssert(m_lightIDToShadowRecords.contains(deletedID), "Failed to find the light ID");
				m_lightIDToShadowRecords.erase(deletedID);
			}
		}
	}


	void ShadowsGraphicsSystem::RegisterNewShadowTextureElements(gr::RenderDataManager const& renderData)
	{
		std::vector<gr::RenderDataID> const* newShadows = renderData.GetIDsWithNewData<gr::ShadowMap::RenderData>();
		if (newShadows && !newShadows->empty())
		{
			for (auto const& shadowItr : gr::IDAdapter(renderData, *newShadows))
			{
				const gr::RenderDataID shadowID = shadowItr->GetRenderDataID();

				gr::ShadowMap::RenderData const& shadowMapRenderData = shadowItr->Get<gr::ShadowMap::RenderData>();

				auto AddShadowToMetadata = [&shadowID, this](ShadowTextureMetadata& shadowMetadata)
					{
						SEAssert(!shadowMetadata.m_renderDataIDToTexArrayIdx.contains(shadowID),
							"Shadow is already registered");

						const uint32_t newShadowIndex = shadowMetadata.m_numShadows++;

						shadowMetadata.m_renderDataIDToTexArrayIdx.emplace(shadowID, newShadowIndex);
						shadowMetadata.m_texArrayIdxToRenderDataID.emplace(newShadowIndex, shadowID);

						SEAssert(shadowMetadata.m_renderDataIDToTexArrayIdx.size() == shadowMetadata.m_numShadows &&
							shadowMetadata.m_texArrayIdxToRenderDataID.size() == shadowMetadata.m_numShadows,
							"Number of shadows counter is out of sync");

						// Note: The render data dirty IDs list also contains new object IDs, so we don't need to add new
						// objects to our dirty indexes list here

						// Update the shadow record output:
						SEAssert(m_lightIDToShadowRecords.contains(shadowID) == false, "RenderDataID already registered");
						m_lightIDToShadowRecords.emplace(
							shadowID,
							gr::ShadowRecord{
								.m_shadowTex = &shadowMetadata.m_shadowArray,
								.m_shadowTexArrayIdx = newShadowIndex,
							});
					};

				switch (shadowMapRenderData.m_lightType)
				{
				case gr::Light::Type::Directional: AddShadowToMetadata(m_directionalShadowTexMetadata); break;
				case gr::Light::Type::Point: AddShadowToMetadata(m_pointShadowTexMetadata); break;
				case gr::Light::Type::Spot: AddShadowToMetadata(m_spotShadowTexMetadata); break;
				case gr::Light::Type::AmbientIBL:
				default: SEAssertF("Invalid light type");
				}
			}
		}

		// (Re)Create the backing shadow array textures:
		UpdateShadowTextures(renderData);
	}


	void ShadowsGraphicsSystem::UpdateShadowTextures(gr::RenderDataManager const& renderData)
	{
		auto UpdateShadowTexture = [this](
			gr::Light::Type lightType,
			ShadowTextureMetadata& shadowMetadata,
			char const* shadowTexName)
			{
				// If the buffer does not exist we must create it:
				bool mustReallocate = shadowMetadata.m_shadowArray == nullptr;

				if (!mustReallocate)
				{
					const uint32_t curNumTexArrayElements = shadowMetadata.m_shadowArray->GetTextureParams().m_arraySize;

					// If the buffer is too small, or if the no. of lights has shrunk by too much, we must reallocate:
					mustReallocate = shadowMetadata.m_numShadows > 0 &&
						(shadowMetadata.m_numShadows > curNumTexArrayElements ||
							shadowMetadata.m_numShadows <= curNumTexArrayElements * k_shrinkReallocationFactor);
				}

				if (mustReallocate)
				{
					re::Texture::TextureParams shadowArrayParams;

					util::CHashKey const* shadowUpdateEventName = nullptr;

					switch (lightType)
					{
					case gr::Light::Directional:
					{
						const int defaultDirectionalWidthHeight =
							core::Config::GetValue<int>(core::configkeys::k_defaultDirectionalShadowMapResolutionKey);

						shadowArrayParams.m_width = defaultDirectionalWidthHeight;
						shadowArrayParams.m_height = defaultDirectionalWidthHeight;
						shadowArrayParams.m_dimension = re::Texture::Dimension::Texture2DArray;
						
						shadowUpdateEventName = &greventkey::GS_Shadows_DirectionalShadowArrayUpdated;
					}
					break;
					case gr::Light::Point:
					{
						const int defaultCubemapWidthHeight =
							core::Config::GetValue<int>(core::configkeys::k_defaultShadowCubeMapResolutionKey);

						shadowArrayParams.m_width = defaultCubemapWidthHeight;
						shadowArrayParams.m_height = defaultCubemapWidthHeight;
						shadowArrayParams.m_dimension = re::Texture::Dimension::TextureCubeArray;

						shadowUpdateEventName = &greventkey::GS_Shadows_PointShadowArrayUpdated;
					}
					break;
					case gr::Light::Spot:
					{
						const int defaultSpotWidthHeight =
							core::Config::GetValue<int>(core::configkeys::k_defaultSpotShadowMapResolutionKey);

						shadowArrayParams.m_width = defaultSpotWidthHeight;
						shadowArrayParams.m_height = defaultSpotWidthHeight;
						shadowArrayParams.m_dimension = re::Texture::Dimension::Texture2DArray;

						shadowUpdateEventName = &greventkey::GS_Shadows_SpotShadowArrayUpdated;
					}
					break;
					case gr::Light::AmbientIBL:
					default: SEAssertF("Invalid light type");
					}

					shadowArrayParams.m_arraySize = std::max(1u, shadowMetadata.m_numShadows);

					LOG(std::format("Creating {} shadow array texture with {} elements",
						gr::Light::LightTypeToCStr(lightType), shadowArrayParams.m_arraySize));

					shadowArrayParams.m_usage = re::Texture::Usage::DepthTarget | re::Texture::Usage::ColorSrc;

					shadowArrayParams.m_format = re::Texture::Format::Depth32F;
					shadowArrayParams.m_colorSpace = re::Texture::ColorSpace::Linear;
					shadowArrayParams.m_mipMode = re::Texture::MipMode::None;
					shadowArrayParams.m_optimizedClear.m_depthStencil.m_depth = 1.f;

					// Cache the current shadow texture address before we replace it:
					core::InvPtr<re::Texture> const* prevShadowTex = &shadowMetadata.m_shadowArray;

					shadowMetadata.m_shadowArray = re::Texture::Create(shadowTexName, shadowArrayParams);

					// Update the existing shadow record outputs with the new texture:
					uint32_t newArrayIdx = 0;
					for (auto& entry : m_lightIDToShadowRecords)
					{
						if (entry.second.m_shadowTex == prevShadowTex)
						{
							SEAssert(newArrayIdx < shadowArrayParams.m_arraySize,
								"New shadow texture array index is out of bounds");
							entry.second.m_shadowTex = &shadowMetadata.m_shadowArray;
							entry.second.m_shadowTexArrayIdx = newArrayIdx++;
						}
					}

					// Post an event to notify other systems that the shadow texture has been updated:
					m_graphicsSystemManager->PostGraphicsEvent<ShadowsGraphicsSystem>(
						*shadowUpdateEventName, true); // Arbitrary: Need a value
				}
			};
		UpdateShadowTexture(gr::Light::Directional, m_directionalShadowTexMetadata, "Directional shadows");
		UpdateShadowTexture(gr::Light::Point, m_pointShadowTexMetadata, "Point shadows");
		UpdateShadowTexture(gr::Light::Spot, m_spotShadowTexMetadata, "Spot shadows");
	}


	void ShadowsGraphicsSystem::ShowImGuiWindow()
	{
		constexpr ImGuiTableFlags k_tableFlags =
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable;

		auto ShowShadowMetadata = [](ShadowTextureMetadata const& shadowMetadata)
			{
				ImGui::Indent();
				ImGui::Text(std::format("No. of shadows: {}", shadowMetadata.m_numShadows).c_str());
				ImGui::Text(std::format("Shadow array size: {}",
					shadowMetadata.m_shadowArray->GetTextureParams().m_arraySize).c_str());
				ImGui::Text(std::format("Shadow array element width: {}",
					shadowMetadata.m_shadowArray->GetTextureParams().m_width).c_str());
				ImGui::Text(std::format("Shadow array element height: {}",
					shadowMetadata.m_shadowArray->GetTextureParams().m_height).c_str());
				ImGui::Unindent();
			};


		if (ImGui::CollapsingHeader("Directional Lights", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShowShadowMetadata(m_directionalShadowTexMetadata);
		}

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShowShadowMetadata(m_pointShadowTexMetadata);
		}

		ImGui::NewLine();

		if (ImGui::CollapsingHeader("Spot Lights", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ShowShadowMetadata(m_spotShadowTexMetadata);
		}
	}
}