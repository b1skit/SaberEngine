// © 2022 Adam Badke. All rights reserved.
#include "AccelerationStructure.h"
#include "BatchBuilder.h"
#include "BatchFactories.h"
#include "Buffer.h"
#include "Effect.h"
#include "GraphicsEvent.h"
#include "GraphicsSystem_DeferredLightVolumes.h"
#include "GraphicsSystem_GBuffer.h"
#include "GraphicsSystemCommon.h"
#include "GraphicsSystemManager.h"
#include "IndexedBuffer.h"
#include "LightRenderData.h"
#include "RayTracingParamsHelpers.h"
#include "RenderDataManager.h"
#include "Sampler.h"
#include "Stage.h"
#include "TextureTarget.h"
#include "TextureView.h"

#include "Core/Assert.h"
#include "Core/Config.h"

#include "Core/Util/CHashKey.h"
#include "Core/Util/HashKey.h"

#include "Renderer/Shaders/Common/InstancingParams.h"
#include "Renderer/Shaders/Common/LightParams.h"
#include "Renderer/Shaders/Common/RayTracingParams.h"
#include "Renderer/Shaders/Common/ShadowParams.h"
#include "Renderer/Shaders/Common/TransformParams.h"


namespace
{
	static const EffectID k_deferredLightingEffectID = effect::Effect::ComputeEffectID("DeferredLighting");

	static const util::HashKey k_sampler2DShadowName("BorderCmpMinMagLinearMipPoint");
	static const util::HashKey k_samplerCubeShadowName("WrapCmpMinMagLinearMipPoint");

	static constexpr char const* k_directionalShadowShaderName = "DirectionalShadows";
	static constexpr char const* k_pointShadowShaderName = "PointShadows";
	static constexpr char const* k_spotShadowShaderName = "SpotShadows";


	re::TextureView CreateShadowArrayReadView(core::InvPtr<re::Texture> const& shadowArray)
	{
		return re::TextureView(
			shadowArray,
			{ re::TextureView::ViewFlags::ReadOnlyDepth });
	}


	void AttachGBufferInputs(
		gr::GraphicsSystemManager* gsm, gr::TextureDependencies const& texDependencies, gr::Stage* stage)
	{
		core::InvPtr<re::Sampler> const& wrapMinMagLinearMipPoint = gsm->GetSampler("WrapMinMagLinearMipPoint");

		for (uint8_t slot = 0; slot < gr::GBufferGraphicsSystem::GBufferTexIdx::GBufferTexIdx_Count; slot++)
		{
			if (slot == gr::GBufferGraphicsSystem::GBufferEmissive)
			{
				continue; // The emissive texture is not used
			}
			SEAssert(texDependencies.contains(gr::GBufferGraphicsSystem::GBufferTexNameHashKeys[slot]),
				"Texture dependency not found");

			util::CHashKey const& texName = gr::GBufferGraphicsSystem::GBufferTexNameHashKeys[slot];
			core::InvPtr<re::Texture> const& gbufferTex =
				*gr::GetDependency<core::InvPtr<re::Texture>>(texName, texDependencies);

			stage->AddPermanentTextureInput(
				texName.GetKey(), gbufferTex, wrapMinMagLinearMipPoint, re::TextureView(gbufferTex));
		}
	}
}


namespace gr
{
	DeferredLightVolumeGraphicsSystem::DeferredLightVolumeGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(GetScriptName(), owningGSM)
		, INamedObject(GetScriptName())
		, m_shadowMode(ShadowMode::Invalid)
		, m_pointCullingResults(nullptr)
		, m_spotCullingResults(nullptr)
		, m_lightIDToShadowRecords(nullptr)
		, m_PCSSSampleParamsBuffer(nullptr)
		, m_lightingTargetTex(nullptr)
		, m_directionalShadowTexArrayUpdated(false)
		, m_pointShadowTexArrayUpdated(false)
		, m_spotShadowTexArrayUpdated(false)
		, m_sceneTLAS(nullptr)
		, m_tMin(0.01f)
		, m_rayLengthOffset(0.01f)
		, m_geometryInstanceMask(re::AccelerationStructure::InstanceInclusionMask_Always)
	{
		m_lightingTargetSet = re::TextureTargetSet::Create("Deferred light targets");
	}


	void DeferredLightVolumeGraphicsSystem::RegisterInputs()
	{
		RegisterTextureInput(k_lightingTargetTexInput);

		// Deferred lighting GS is (currently) tightly coupled to the GBuffer GS
		for (uint8_t slot = 0; slot < GBufferGraphicsSystem::GBufferTexIdx_Count; slot++)
		{
			if (slot == GBufferGraphicsSystem::GBufferEmissive)
			{
				continue;
			}

			RegisterTextureInput(GBufferGraphicsSystem::GBufferTexNameHashKeys[slot]);
		}

		RegisterDataInput(k_pointLightCullingDataInput);
		RegisterDataInput(k_spotLightCullingDataInput);

		// Shadow-related inputs:
		m_shadowMode = core::Config::KeyExists(util::CHashKey(core::configkeys::k_raytracingKey)) ?
			ShadowMode::RayTraced : ShadowMode::ShadowMap;
		switch (m_shadowMode)
		{
		case ShadowMode::ShadowMap:
		{
			RegisterDataInput(k_lightIDToShadowRecordInput);
			RegisterBufferInput(k_PCSSSampleParamsBufferInput);
		}
		break;
		case ShadowMode::RayTraced:
		{
			RegisterDataInput(k_sceneTLASInput);
		}
		break;
		default: SEAssertF("Invalid shadow mode flag");
		};
	}


	void DeferredLightVolumeGraphicsSystem::RegisterOutputs()
	{
		//
	}


	void DeferredLightVolumeGraphicsSystem::InitCommonPipeline(
		gr::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const& bufferDependencies,
		DataDependencies const& dataDependencies)
	{
		SEAssert(texDependencies.contains(k_lightingTargetTexInput), "Missing a mandatory dependency");

		// Cache our dependencies:
		m_lightingTargetTex = GetDependency<core::InvPtr<re::Texture>>(k_lightingTargetTexInput, texDependencies);

		switch (m_shadowMode)
		{
		case ShadowMode::ShadowMap:
		{
			m_lightIDToShadowRecords = GetDependency<LightIDToShadowRecordMap>(k_lightIDToShadowRecordInput, dataDependencies);
			m_PCSSSampleParamsBuffer = GetDependency<std::shared_ptr<re::Buffer>>(k_PCSSSampleParamsBufferInput, bufferDependencies);

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
		}
		break;
		case ShadowMode::RayTraced:
		{
			m_sceneTLAS = GetDependency<TLAS>(k_sceneTLASInput, dataDependencies);
		}
		break;
		default: SEAssertF("Invalid shadow mode");
		};

		// Create the lighting target set:
		m_lightingTargetSet->SetColorTarget(
			0,
			*m_lightingTargetTex,
			re::TextureTarget::TargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1) });

		// We need the depth buffer attached, but with depth writes disabled:
		re::TextureTarget::TargetParams depthTargetParams{ .m_textureView = {
				re::TextureView::Texture2DView(0, 1),
				{re::TextureView::ViewFlags::ReadOnlyDepth} } };

		m_lightingTargetSet->SetDepthStencilTarget(
			*texDependencies.at(GBufferGraphicsSystem::GBufferTexNameHashKeys[GBufferGraphicsSystem::GBufferDepth]),
			depthTargetParams);
	}


	void DeferredLightVolumeGraphicsSystem::InitDirectionalLightPipeline(
		gr::StagePipeline& pipeline, TextureDependencies const& texDependencies, BufferDependencies const&, DataDependencies const&)
	{
		// Directional light stage:
		//-------------------------
		m_directionalStage = gr::Stage::CreateGraphicsStage("Directional light stage", gr::Stage::GraphicsStageParams{});

		m_directionalStage->SetInstancingEnabled(false); // TODO: Enable instancing for deferred light mesh batches
		
		m_directionalStage->SetTextureTargetSet(m_lightingTargetSet);

		m_directionalStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_DeferredDirectional);

		m_directionalStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		switch (m_shadowMode)
		{
		case ShadowMode::ShadowMap:
		{
			m_directionalStage->AddPermanentBuffer(PoissonSampleParamsData::s_shaderName, *m_PCSSSampleParamsBuffer);
		}
		break;
		case ShadowMode::RayTraced:
		{
			m_directionalStage->AddDrawStyleBits(effect::drawstyle::ShadowMode_RayTraced);
		}
		break;
		default: SEAssertF("Invalid shadow mode");
		};

		AttachGBufferInputs(m_graphicsSystemManager, texDependencies, m_directionalStage.get());
		
		pipeline.AppendStage(m_directionalStage);

		// Register for events:
		m_graphicsSystemManager->SubscribeToGraphicsEvent<DeferredLightVolumeGraphicsSystem>(
			greventkey::GS_Shadows_DirectionalShadowArrayUpdated, this);
	}


	void DeferredLightVolumeGraphicsSystem::InitPointLightPipeline(
		gr::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const&, 
		DataDependencies const& dataDependencies)
	{
		// Point light stage:
		//-------------------
		m_pointStage = gr::Stage::CreateGraphicsStage("Point light stage", gr::Stage::GraphicsStageParams{});

		m_pointStage->SetInstancingEnabled(false); // TODO: Enable instancing for deferred light mesh batches

		m_pointStage->SetTextureTargetSet(m_lightingTargetSet);
		m_pointStage->AddPermanentBuffer(m_lightingTargetSet->GetCreateTargetParamsBuffer());
		m_pointStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		m_pointStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_DeferredPoint);

		switch (m_shadowMode)
		{
		case ShadowMode::ShadowMap:
		{
			m_pointCullingResults = GetDependency<PunctualLightCullingResults>(k_pointLightCullingDataInput, dataDependencies);
			
			m_pointStage->AddPermanentBuffer(PoissonSampleParamsData::s_shaderName, *m_PCSSSampleParamsBuffer);
		}
		break;
		case ShadowMode::RayTraced:
		{
			m_pointStage->AddDrawStyleBits(effect::drawstyle::ShadowMode_RayTraced);
		}
		break;
		default: SEAssertF("Invalid shadow mode");
		};

		AttachGBufferInputs(m_graphicsSystemManager, texDependencies, m_pointStage.get());

		pipeline.AppendStage(m_pointStage);

		// Register for events:
		m_graphicsSystemManager->SubscribeToGraphicsEvent<DeferredLightVolumeGraphicsSystem>(
			greventkey::GS_Shadows_PointShadowArrayUpdated, this);
	}


	void DeferredLightVolumeGraphicsSystem::InitSpotLightPipeline(
		gr::StagePipeline& pipeline,
		TextureDependencies const& texDependencies,
		BufferDependencies const&,
		DataDependencies const& dataDependencies)
	{
		// Spot light stage:
		//------------------
		m_spotStage = gr::Stage::CreateGraphicsStage("Spot light stage", gr::Stage::GraphicsStageParams{});

		m_spotStage->SetInstancingEnabled(false); // TODO: Enable instancing for deferred light mesh batches
		
		m_spotStage->SetTextureTargetSet(m_lightingTargetSet);
		m_spotStage->AddPermanentBuffer(m_lightingTargetSet->GetCreateTargetParamsBuffer());
		m_spotStage->AddPermanentBuffer(m_graphicsSystemManager->GetActiveCameraParams());

		m_spotStage->AddDrawStyleBits(effect::drawstyle::DeferredLighting_DeferredSpot);

		switch (m_shadowMode)
		{
		case ShadowMode::ShadowMap:
		{
			m_spotCullingResults = GetDependency<PunctualLightCullingResults>(k_spotLightCullingDataInput, dataDependencies);

			m_spotStage->AddPermanentBuffer(PoissonSampleParamsData::s_shaderName, *m_PCSSSampleParamsBuffer);
		}
		break;
		case ShadowMode::RayTraced:
		{
			m_spotStage->AddDrawStyleBits(effect::drawstyle::ShadowMode_RayTraced);
		}
		break;
		default: SEAssertF("Invalid shadow mode");
		};

		AttachGBufferInputs(m_graphicsSystemManager, texDependencies, m_spotStage.get());

		pipeline.AppendStage(m_spotStage);

		// Register for events:
		m_graphicsSystemManager->SubscribeToGraphicsEvent<DeferredLightVolumeGraphicsSystem>(
			greventkey::GS_Shadows_SpotShadowArrayUpdated, this);
	}


	void DeferredLightVolumeGraphicsSystem::PreRender()
	{
		HandleEvents();

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		gr::IndexedBufferManager& ibm = renderData.GetInstancingIndexedBufferManager();

		// Removed any deleted directional/point/spot lights:
		auto DeleteLights = [](
			std::vector<gr::RenderDataID> const* deletedIDs,
			std::unordered_map<gr::RenderDataID, PunctualLightData>& stageData)
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
		
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataDirectional>(), m_punctualLightData);
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataPoint>(), m_punctualLightData);
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataSpot>(), m_punctualLightData);


		// If the shadow array texture was recreated, we need to recreate all light batches. Otherwise, we only need
		// to create batches for any new lights
		auto RegisterNewLight = [&](
			auto const& lightItr,
			gr::Light::Type lightType,
			void const* lightRenderData,
			bool hasShadow,
			std::unordered_map<gr::RenderDataID, PunctualLightData>& punctualLightData)
			{
				const gr::RenderDataID lightID = lightItr->GetRenderDataID();

				gr::RasterBatchBuilder batchBuilder = gr::RasterBatchBuilder::CreateInstance(
					lightID, renderData, grutil::BuildInstancedRasterBatch)
					.SetEffectID(k_deferredLightingEffectID);

				if (hasShadow && m_shadowMode == ShadowMode::ShadowMap)
				{
					SEAssert(m_lightIDToShadowRecords->contains(lightID), "Failed to find a shadow record");
					gr::ShadowRecord const& shadowRecord = m_lightIDToShadowRecords->at(lightID);

					switch (lightType)
					{
					case gr::Light::Type::Directional:
					{
						SEAssert(m_lightIDToShadowRecords->contains(lightID), "Failed to find a shadow record");
						gr::ShadowRecord const& shadowRecord = m_lightIDToShadowRecords->at(lightID);

						std::move(batchBuilder).SetTextureInput(k_directionalShadowShaderName,
							*shadowRecord.m_shadowTex,
							m_graphicsSystemManager->GetSampler(k_sampler2DShadowName),
							CreateShadowArrayReadView(*shadowRecord.m_shadowTex));
					}
					break;
					case gr::Light::Type::Point:
					{
						std::move(batchBuilder).SetTextureInput(k_pointShadowShaderName,
							*shadowRecord.m_shadowTex,
							m_graphicsSystemManager->GetSampler(k_samplerCubeShadowName),
							CreateShadowArrayReadView(*shadowRecord.m_shadowTex));
					}
					break;
					case gr::Light::Type::Spot:
					{
						std::move(batchBuilder).SetTextureInput(k_spotShadowShaderName,
							*shadowRecord.m_shadowTex,
							m_graphicsSystemManager->GetSampler(k_sampler2DShadowName),
							CreateShadowArrayReadView(*shadowRecord.m_shadowTex));
					}
					break;
					default: SEAssertF("Invalid light type for this function");
					}
				}

				// Create/update the punctual light data record:
				punctualLightData[lightID] = PunctualLightData{
					.m_type = lightType,
					.m_batch = std::move(batchBuilder).Build(),
					.m_hasShadow = hasShadow
				};
			};

		// Directional:
		std::vector<gr::RenderDataID> const* dirlLightIDsForNewBatch = nullptr;
		if (m_directionalShadowTexArrayUpdated)
		{
			dirlLightIDsForNewBatch = renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataDirectional>();
		}
		else
		{
			dirlLightIDsForNewBatch = renderData.GetIDsWithNewData<gr::Light::RenderDataDirectional>();
		}
		if (dirlLightIDsForNewBatch && !dirlLightIDsForNewBatch->empty())
		{
			for (auto const& directionalItr : gr::IDAdapter(renderData, *dirlLightIDsForNewBatch))
			{
				gr::Light::RenderDataDirectional const& directionalData =
					directionalItr->Get<gr::Light::RenderDataDirectional>();

				const bool hasShadow = directionalData.m_hasShadow;

				RegisterNewLight(
					directionalItr, gr::Light::Directional, &directionalData, hasShadow, m_punctualLightData);
			}
		}

		// Point:
		std::vector<gr::RenderDataID> const* pointlLightIDsForNewBatch = nullptr;
		if (m_pointShadowTexArrayUpdated)
		{
			pointlLightIDsForNewBatch = renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataPoint>();
		}
		else
		{
			pointlLightIDsForNewBatch = renderData.GetIDsWithNewData<gr::Light::RenderDataPoint>();
		}
		if (pointlLightIDsForNewBatch && !pointlLightIDsForNewBatch->empty())
		{
			for (auto const& pointItr : gr::IDAdapter(renderData, *pointlLightIDsForNewBatch))
			{
				gr::Light::RenderDataPoint const& pointData = pointItr->Get<gr::Light::RenderDataPoint>();
				const bool hasShadow = pointData.m_hasShadow;

				RegisterNewLight(pointItr, gr::Light::Point, &pointData, hasShadow, m_punctualLightData);
			}
		}

		// Spot:
		std::vector<gr::RenderDataID> const* spotlLightIDsForNewBatch = nullptr;
		if (m_spotShadowTexArrayUpdated)
		{
			spotlLightIDsForNewBatch = renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataSpot>();
		}
		else
		{
			spotlLightIDsForNewBatch = renderData.GetIDsWithNewData<gr::Light::RenderDataSpot>();
		}
		if (spotlLightIDsForNewBatch && !spotlLightIDsForNewBatch->empty())
		{
			for (auto const& spotItr : gr::IDAdapter(renderData, *spotlLightIDsForNewBatch))
			{
				gr::Light::RenderDataSpot const& spotData = spotItr->Get<gr::Light::RenderDataSpot>();
				const bool hasShadow = spotData.m_hasShadow;

				RegisterNewLight(spotItr, gr::Light::Spot, &spotData, hasShadow, m_punctualLightData);
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

		switch (m_shadowMode)
		{
		case ShadowMode::ShadowMap:
		{
			//
		}
		break;
		case ShadowMode::RayTraced:
		{
			std::shared_ptr<re::Buffer> const& traceRayInlineParams = grutil::CreateTraceRayInlineParams(
				m_geometryInstanceMask,
				RayFlag::AcceptFirstHitAndEndSearch | RayFlag::SkipClosestHitShader | CullBackFacingTriangles,
				m_tMin,
				m_rayLengthOffset);

			m_directionalStage->AddSingleFrameBuffer("TraceRayInlineParams", traceRayInlineParams);
			m_pointStage->AddSingleFrameBuffer("TraceRayInlineParams", traceRayInlineParams);
			m_spotStage->AddSingleFrameBuffer("TraceRayInlineParams", traceRayInlineParams);

			m_directionalStage->AddSingleFrameTLAS(re::ASInput("SceneBVH", *m_sceneTLAS));
			m_pointStage->AddSingleFrameTLAS(re::ASInput("SceneBVH", *m_sceneTLAS));
			m_spotStage->AddSingleFrameTLAS(re::ASInput("SceneBVH", *m_sceneTLAS));
		}
		break;
		default: SEAssertF("Invalid shadow mode");
		};

		CreateBatches();
	}


	void DeferredLightVolumeGraphicsSystem::CreateBatches()
	{
		// TODO: Instance deferred mesh lights draws via a single batch
	
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		gr::IndexedBufferManager& ibm = renderData.GetInstancingIndexedBufferManager();
		
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
		else if (renderData.HasObjectData<gr::Light::RenderDataSpot>())
		{
			MarkAllIDsVisible(gr::IDAdapter(renderData, *renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataSpot>()));			
		}

		if (m_pointCullingResults)
		{
			MarkIDsVisible(m_pointCullingResults);
		}
		else if (renderData.HasObjectData<gr::Light::RenderDataPoint>())
		{
			MarkAllIDsVisible(gr::IDAdapter(renderData, *renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataPoint>()));
		}


		// Update all of the punctual lights we're tracking:
		for (auto& lightData : m_punctualLightData)
		{
			const gr::RenderDataID lightID = lightData.first;

			// Update lighting buffers, if anything is dirty:
			const bool lightRenderDataDirty = 
				(lightData.second.m_type == gr::Light::Type::Directional &&
					renderData.IsDirty<gr::Light::RenderDataDirectional>(lightID)) ||
				(lightData.second.m_type == gr::Light::Type::Point &&
					renderData.IsDirty<gr::Light::RenderDataPoint>(lightID)) ||
				(lightData.second.m_type == gr::Light::Type::Spot &&
					renderData.IsDirty<gr::Light::RenderDataSpot>(lightID));

			if (lightRenderDataDirty)
			{
				switch (lightData.second.m_type)
				{
				case gr::Light::Type::Directional:
				{
					gr::Light::RenderDataDirectional const& directionalData =
						renderData.GetObjectData<gr::Light::RenderDataDirectional>(lightID);
					lightData.second.m_canContribute = directionalData.m_canContribute;
				}
				break;
				case gr::Light::Type::Point:
				{
					gr::Light::RenderDataPoint const& pointData = 
						renderData.GetObjectData<gr::Light::RenderDataPoint>(lightID);
					lightData.second.m_canContribute = pointData.m_canContribute;
				}
				break;
				case gr::Light::Type::Spot:
				{
					gr::Light::RenderDataSpot const& spotData =
						renderData.GetObjectData<gr::Light::RenderDataSpot>(lightID);
					lightData.second.m_canContribute = spotData.m_canContribute;
				}
				break;
				default: SEAssertF("Invalid light type");
				}
			}

			
			// Add punctual batches:
			if (lightData.second.m_canContribute &&
				(lightData.second.m_type == gr::Light::Type::Directional || 
					visibleLightIDs.contains(lightID)))
			{
				auto AddBatch = [&lightData, &lightID, &ibm, this](gr::Stage* stage)
					{
						gr::StageBatchHandle& duplicatedBatch = *stage->AddBatch(lightData.second.m_batch);

						uint32_t shadowTexArrayIdx = INVALID_SHADOW_IDX;
						if (lightData.second.m_hasShadow && m_shadowMode == ShadowMode::ShadowMap)
						{
							SEAssert(m_lightIDToShadowRecords->contains(lightID), "Failed to find a shadow record");
							gr::ShadowRecord const& shadowRecord = m_lightIDToShadowRecords->at(lightID);

							shadowTexArrayIdx = shadowRecord.m_shadowTexArrayIdx;
						}

						char const* lutShaderName = nullptr;
						switch (lightData.second.m_type)
						{
						case gr::Light::Type::Directional:
						{
							lutShaderName = LightShadowLUTData::s_shaderNameDirectional;
						}
						break;
						case gr::Light::Type::Point:
						{
							lutShaderName = LightShadowLUTData::s_shaderNamePoint;

							// Add the Transform and instanced index LUT:
							duplicatedBatch.SetSingleFrameBuffer(
								ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));

							duplicatedBatch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
								InstanceIndexData::s_shaderName, std::views::single(lightID)));
						}
						break;
						case gr::Light::Type::Spot:
						{
							lutShaderName = LightShadowLUTData::s_shaderNameSpot;

							// Add the Transform and instanced index LUT:
							duplicatedBatch.SetSingleFrameBuffer(
								ibm.GetIndexedBufferInput(TransformData::s_shaderName, TransformData::s_shaderName));

							duplicatedBatch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<InstanceIndexData>(
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
								lightData.second.m_type),
						};

						duplicatedBatch.SetSingleFrameBuffer(ibm.GetLUTBufferInput<LightShadowLUTData>(
							lutShaderName,
							{ lightShadowLUT },
							std::span<const gr::RenderDataID>{&lightID, 1}));
					};

				
				switch (lightData.second.m_type)
				{
				case gr::Light::Type::Directional:
				{
					AddBatch(m_directionalStage.get());
				}
				break;
				case gr::Light::Type::Point:
				{
					AddBatch(m_pointStage.get());
				}
				break;
				case gr::Light::Type::Spot:
				{
					AddBatch(m_spotStage.get());
				}
				break;
				case gr::Light::Type::AmbientIBL:
				default: SEAssertF("Invalid light type");
				}
			}
		}
	}


	void DeferredLightVolumeGraphicsSystem::HandleEvents()
	{
		m_directionalShadowTexArrayUpdated = false;
		m_pointShadowTexArrayUpdated = false;
		m_spotShadowTexArrayUpdated = false;

		while (HasEvents())
		{
			gr::GraphicsEvent const& event = GetEvent();
			switch (event.m_eventKey)
			{
			case greventkey::GS_Shadows_DirectionalShadowArrayUpdated:
			{
				m_directionalShadowTexArrayUpdated = true;
			}
			break;
			case greventkey::GS_Shadows_PointShadowArrayUpdated:
			{
				m_pointShadowTexArrayUpdated = true;
			}
			break;
			case greventkey::GS_Shadows_SpotShadowArrayUpdated:
			{
				m_spotShadowTexArrayUpdated = true;
			}
			break;
			default: SEAssertF("Unexpected event key");
			}
		}
	}


	void DeferredLightVolumeGraphicsSystem::ShowImGuiWindow()
	{
		if (m_shadowMode == ShadowMode::RayTraced)
		{
			ImGui::SliderFloat("Shadow ray tMin", &m_tMin, 0.f, 1.f);
			ImGui::SliderFloat("Shadow ray length offset", &m_rayLengthOffset, 0.f, 1.f);
		}		
	}
}