// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystemManager.h"
#include "LightManager.h"
#include "LightRenderData.h"
#include "RenderManager.h"
#include "RenderDataManager.h"
#include "ShadowMapRenderData.h"

#include "Core/Config.h"

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


	std::vector<gr::RenderDataID> CombineVisibleRenderDataIDs(
		gr::RenderDataManager const& renderData,
		gr::GraphicsSystem::ViewCullingResults const& viewCullingResults,
		std::vector<gr::Camera::View> const& views)
	{
		// TODO: This function is a temporary convenience, we concatenate sets of RenderDataIDs together for point light
		// shadow draws which use a geometry shader to project each batch to every face. This is (potentially?) wasteful!
		// Instead, we should draw each point light shadow face seperately, using the culling results to send only the
		// relevant batches to each face.

		const size_t numMeshPrimitives = renderData.GetNumElementsOfType<gr::MeshPrimitive::RenderData>();
		std::vector<gr::RenderDataID> uniqueRenderDataIDs;
		uniqueRenderDataIDs.reserve(numMeshPrimitives);

		// Combine the RenderDataIDs visible in each view into a unique set
		std::unordered_set<gr::RenderDataID> seenIDs;
		seenIDs.reserve(numMeshPrimitives);

		for (gr::Camera::View const& view : views)
		{
			std::vector<gr::RenderDataID> const& visibleIDs = viewCullingResults.at(view);
			for (gr::RenderDataID id : visibleIDs)
			{
				if (!seenIDs.contains(id))
				{
					seenIDs.emplace(id);
					uniqueRenderDataIDs.emplace_back(id);
				}
			}
		}

		return uniqueRenderDataIDs;
	}
}


namespace gr
{
	ShadowsGraphicsSystem::ShadowsGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_stagePipeline(nullptr)
	{
	}


	void ShadowsGraphicsSystem::RegisterInputs()
	{
		RegisterDataInput(k_cullingDataInput);
		RegisterDataInput(k_pointLightCullingDataInput);
		RegisterDataInput(k_spotLightCullingDataInput);
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

		// Shadow map array target texture:
		gr::LightManager const& lightManager = re::RenderManager::Get()->GetLightManager();
		std::shared_ptr<re::Texture> cubeShadowArrayTex = lightManager.GetShadowArrayTexture(shadowData.m_lightType);

		// Texture target set:
		std::shared_ptr<re::TextureTargetSet> pointShadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_CubeShadowTargetSet", lightName));

		pointShadowTargetSet->SetDepthStencilTarget(
			cubeShadowArrayTex, 
			re::TextureTarget::TargetParams{
				.m_textureView = lightManager.GetShadowWriteView(shadowData.m_lightType, lightID) });

		pointShadowTargetSet->SetViewport(lightManager.GetShadowArrayWriteViewport(shadowData.m_lightType));
		pointShadowTargetSet->SetScissorRect(lightManager.GetShadowArrayWriteScissorRect(shadowData.m_lightType));


		pointShadowTargetSet->SetDepthTargetClearMode(re::TextureTarget::ClearMode::Enabled);

		shadowStage->SetTextureTargetSet(pointShadowTargetSet);

		// Cubemap shadow buffer:
		CubemapShadowRenderData const& cubemapShadowParams = GetCubemapShadowRenderParamsData(camData, transformData);

		std::shared_ptr<re::Buffer> cubeShadowBuf = re::Buffer::Create(
			CubemapShadowRenderData::s_shaderName,
			cubemapShadowParams,
			re::Buffer::BufferParams{
				.m_cpuAllocationType = re::Buffer::CPUAllocation::Mutable,
				.m_memPoolPreference = re::Buffer::MemoryPoolPreference::Upload,
				.m_usageMask = re::Buffer::Usage::GPURead | re::Buffer::Usage::CPUWrite,
				.m_type = re::Buffer::Type::Constant,
			});

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
		shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::AlphaBlended, re::RenderStage::FilterMode::Exclude, true);

		// Shadow camera buffer:
		std::shared_ptr<re::Buffer> shadowCamParams = re::Buffer::Create(
			CameraData::s_shaderName,
			shadowCamData.m_cameraParams,
			re::Buffer::BufferParams{
				.m_cpuAllocationType = re::Buffer::CPUAllocation::Mutable,
				.m_memPoolPreference = re::Buffer::MemoryPoolPreference::Upload,
				.m_usageMask = re::Buffer::Usage::GPURead | re::Buffer::Usage::CPUWrite,
				.m_type = re::Buffer::Type::Constant,
			});

		shadowStage->AddPermanentBuffer(shadowCamParams);

		shadowStage->SetDrawStyle(effect::drawstyle::Shadow_2D);

		// Shadow map array target texture:
		gr::LightManager const& lightManager = re::RenderManager::Get()->GetLightManager();
		std::shared_ptr<re::Texture> shadowArrayTex = lightManager.GetShadowArrayTexture(shadowData.m_lightType);

		// Texture target set:
		std::shared_ptr<re::TextureTargetSet> shadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_2DShadowTargetSet", lightName));

		shadowTargetSet->SetDepthStencilTarget(
			shadowArrayTex, 
			re::TextureTarget::TargetParams{
				.m_textureView = lightManager.GetShadowWriteView(shadowData.m_lightType, lightID) });

		shadowTargetSet->SetViewport(lightManager.GetShadowArrayWriteViewport(shadowData.m_lightType));
		shadowTargetSet->SetScissorRect(lightManager.GetShadowArrayWriteScissorRect(shadowData.m_lightType));

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
		re::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&)
	{
		m_stagePipeline = &pipeline;

		std::shared_ptr<re::RenderStage> directionalParentStage = 
			re::RenderStage::CreateParentStage("Directional shadow stages");
		m_directionalParentStageItr = pipeline.AppendRenderStage(directionalParentStage);

		std::shared_ptr<re::RenderStage> pointParentStage = re::RenderStage::CreateParentStage("Point shadow stages");
		m_pointParentStageItr = pipeline.AppendRenderStage(pointParentStage);

		std::shared_ptr<re::RenderStage> spotParentStage = re::RenderStage::CreateParentStage("Spot shadow stages");
		m_spotParentStageItr = pipeline.AppendRenderStage(spotParentStage);
	}


	void ShadowsGraphicsSystem::PreRender(DataDependencies const& dataDependencies)
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		gr::LightManager const& lightManager = re::RenderManager::Get()->GetLightManager();

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
		auto Update2DShadowCamData = [&lightManager](
			gr::Light::Type lightType,
			gr::RenderDataManager::IDIterator const& lightItr,
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

						shadowStageData.m_shadowCamParamBlock->Commit(shadowCamData.m_cameraParams);
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

						m_pointShadowStageData.at(pointItr.GetRenderDataID()).m_shadowCamParamBlock->Commit(
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
				lightManager.GetShadowArrayTexture(gr::Light::Directional),
				lightManager.GetShadowWriteView(gr::Light::Directional, lightID));

			m_stagePipeline->AppendRenderStageForSingleFrame(
				m_directionalParentStageItr, directionalStageItr.second.m_renderStage);
		}
		for (auto& pointStageItr : m_pointShadowStageData)
		{
			const gr::RenderDataID lightID = pointStageItr.first;

			pointStageItr.second.m_renderStage->GetTextureTargetSet()->ReplaceDepthStencilTargetTexture(
				lightManager.GetShadowArrayTexture(gr::Light::Point),
				lightManager.GetShadowWriteView(gr::Light::Point, lightID));

			m_stagePipeline->AppendRenderStageForSingleFrame(
				m_pointParentStageItr, pointStageItr.second.m_renderStage);
		}
		for (auto& spotStageItr : m_spotShadowStageData)
		{
			const gr::RenderDataID lightID = spotStageItr.first;

			spotStageItr.second.m_renderStage->GetTextureTargetSet()->ReplaceDepthStencilTargetTexture(
				lightManager.GetShadowArrayTexture(gr::Light::Spot),
				lightManager.GetShadowWriteView(gr::Light::Spot, lightID));

			m_stagePipeline->AppendRenderStageForSingleFrame(
				m_spotParentStageItr, spotStageItr.second.m_renderStage);
		}

		CreateBatches(dataDependencies);
	}


	void ShadowsGraphicsSystem::CreateBatches(DataDependencies const& dataDependencies)
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		gr::BatchManager const& batchMgr = m_graphicsSystemManager->GetBatchManager();
		
		ViewCullingResults const* cullingResults =
			static_cast<ViewCullingResults const*>(dataDependencies.at(k_cullingDataInput));

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

					if (cullingResults)
					{
						directionalStage.AddBatches(batchMgr.GetSceneBatches(
							cullingResults->at(lightID),
							(gr::BatchManager::InstanceType::Transform | gr::BatchManager::InstanceType::Material),
							re::Batch::Filter::CastsShadow,
							re::Batch::Filter::AlphaBlended));
					}
					else
					{
						directionalStage.AddBatches(batchMgr.GetAllSceneBatches(
							(gr::BatchManager::InstanceType::Transform | gr::BatchManager::InstanceType::Material),
							re::Batch::Filter::CastsShadow,
							re::Batch::Filter::AlphaBlended));
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

							if (cullingResults)
							{
								spotStage.AddBatches(batchMgr.GetSceneBatches(
									cullingResults->at(lightID)));
							}
							else
							{
								spotStage.AddBatches(batchMgr.GetAllSceneBatches());
							}
						}
						++spotItr;
					}
				};

			PunctualLightCullingResults const* spotIDs =
				static_cast<PunctualLightCullingResults const*>(dataDependencies.at(k_spotLightCullingDataInput));
			if (spotIDs)
			{
				auto spotItr = renderData.IDBegin(*spotIDs);
				auto const& spotItrEnd = renderData.IDEnd(*spotIDs);
				AddSpotLightBatches(spotItr, spotItrEnd);
			}
			else
			{
				auto spotItr = renderData.Begin<gr::Light::RenderDataSpot>();
				auto const& spotItrEnd = renderData.End<gr::Light::RenderDataSpot>();
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

							const std::vector<gr::Camera::View> views =
							{
								{pointData.m_renderDataID, gr::Camera::View::Face::XPos},
								{pointData.m_renderDataID, gr::Camera::View::Face::XNeg},
								{pointData.m_renderDataID, gr::Camera::View::Face::YPos},
								{pointData.m_renderDataID, gr::Camera::View::Face::YNeg},
								{pointData.m_renderDataID, gr::Camera::View::Face::ZPos},
								{pointData.m_renderDataID, gr::Camera::View::Face::ZNeg},
							};

							if (cullingResults)
							{
								// TODO: We're currently using a geometry shader to project shadows to cubemap faces, so we need
								// to add all batches to the same stage. This is wasteful, as 5/6 of the faces don't need a
								// given batch. We should draw each face of the cubemap seperately instead
								m_pointShadowStageData.at(pointData.m_renderDataID).m_renderStage->AddBatches(
									batchMgr.GetSceneBatches(
										CombineVisibleRenderDataIDs(renderData, *cullingResults, views)));
							}
							else
							{
								m_pointShadowStageData.at(pointData.m_renderDataID).m_renderStage->AddBatches(
									batchMgr.GetAllSceneBatches());
							}
						}
						++pointItr;
					}
				};

			PunctualLightCullingResults const* pointIDs =
				static_cast<PunctualLightCullingResults const*>(dataDependencies.at(k_pointLightCullingDataInput));
			if (pointIDs)
			{
				auto pointItr = renderData.IDBegin(*pointIDs);
				auto const& pointItrEnd = renderData.IDEnd(*pointIDs);
				AddPointLightBatches(pointItr, pointItrEnd);
			}
			else
			{
				auto pointItr = renderData.Begin<gr::Light::RenderDataPoint>();
				auto const& pointItrEnd = renderData.End<gr::Light::RenderDataPoint>();
				AddPointLightBatches(pointItr, pointItrEnd);

			}
		}
	}
}