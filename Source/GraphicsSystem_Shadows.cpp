// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystemManager.h"
#include "LightRenderData.h"
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

		CubemapShadowRenderData cubemapShadowParams;
		
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
		RegisterDataOutput(k_shadowTexturesOutput, &m_shadowTextures);
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

		shadowStage->SetDrawStyle(effect::DrawStyle::Shadow_Cube);

		// Texture target:
		re::Texture::TextureParams shadowParams;
		shadowParams.m_width = static_cast<uint32_t>(shadowData.m_textureDims.x);
		shadowParams.m_height = static_cast<uint32_t>(shadowData.m_textureDims.y);
		shadowParams.m_usage =
			static_cast<re::Texture::Usage>(re::Texture::Usage::DepthTarget | re::Texture::Usage::Color);
		shadowParams.m_format = re::Texture::Format::Depth32F;
		shadowParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		shadowParams.m_mipMode = re::Texture::MipMode::None;
		shadowParams.m_addToSceneData = false;
		shadowParams.m_clear.m_depthStencil.m_depth = 1.f;
		shadowParams.m_dimension = re::Texture::Dimension::TextureCube;

		std::shared_ptr<re::Texture> depthTexture = 
			re::Texture::Create(std::format("{}_CubeShadow", lightName), shadowParams);

		// Texture target set:
		std::shared_ptr<re::TextureTargetSet> pointShadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_CubeShadowTargetSet", lightName));

		re::TextureTarget::TargetParams depthTargetParams{
			.m_textureView = re::TextureView::Texture2DArrayView(0, 1, 0, 6)};

		pointShadowTargetSet->SetDepthStencilTarget(depthTexture, depthTargetParams);
		pointShadowTargetSet->SetViewport(re::Viewport(0, 0, depthTexture->Width(), depthTexture->Height()));
		pointShadowTargetSet->SetScissorRect(
			{ 0, 0, static_cast<long>(depthTexture->Width()), static_cast<long>(depthTexture->Height()) });

		pointShadowTargetSet->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
			re::TextureTarget::TargetParams::BlendMode::Disabled,
			re::TextureTarget::TargetParams::BlendMode::Disabled });

		pointShadowTargetSet->SetAllColorWriteModes(re::TextureTarget::TargetParams::ChannelWrite{
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled });

		pointShadowTargetSet->SetDepthTargetClearMode(re::TextureTarget::TargetParams::ClearMode::Enabled);

		shadowStage->SetTextureTargetSet(pointShadowTargetSet);

		// Cubemap shadow buffer:
		CubemapShadowRenderData const& cubemapShadowParams = GetCubemapShadowRenderParamsData(camData, transformData);

		std::shared_ptr<re::Buffer> cubeShadowBuf = re::Buffer::Create(
			CubemapShadowRenderData::s_shaderName,
			cubemapShadowParams,
			re::Buffer::Type::Mutable);

		shadowStage->AddPermanentBuffer(cubeShadowBuf);

		dstStageData.emplace(
			lightID,
			ShadowStageData{
				.m_renderStage = shadowStage,
				.m_shadowTargetSet = pointShadowTargetSet,
				.m_shadowCamParamBlock = cubeShadowBuf });

		m_shadowTextures.emplace(lightID, pointShadowTargetSet->GetDepthStencilTarget().GetTexture().get());

		SEAssert(m_shadowTextures.size() ==
			(m_directionalShadowStageData.size() +
				m_pointShadowStageData.size() +
				m_spotShadowStageData.size()),
			"Shadow stage data is out of sync with the aggregate map of all shadow textures");
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

		// Spot shadow camera buffer:
		std::shared_ptr<re::Buffer> shadowCamParams = re::Buffer::Create(
			CameraData::s_shaderName,
			shadowCamData.m_cameraParams,
			re::Buffer::Type::Mutable);

		shadowStage->AddPermanentBuffer(shadowCamParams);

		shadowStage->SetDrawStyle(effect::DrawStyle::Shadow_2D);

		// Texture target:
		re::Texture::TextureParams shadowParams;
		shadowParams.m_width = static_cast<uint32_t>(shadowData.m_textureDims.x);
		shadowParams.m_height = static_cast<uint32_t>(shadowData.m_textureDims.y);
		shadowParams.m_usage =
			static_cast<re::Texture::Usage>(re::Texture::Usage::DepthTarget | re::Texture::Usage::Color);
		shadowParams.m_format = re::Texture::Format::Depth32F;
		shadowParams.m_colorSpace = re::Texture::ColorSpace::Linear;
		shadowParams.m_mipMode = re::Texture::MipMode::None;
		shadowParams.m_addToSceneData = false;
		shadowParams.m_clear.m_depthStencil.m_depth = 1.f;
		shadowParams.m_dimension = re::Texture::Dimension::Texture2D;

		std::shared_ptr<re::Texture> depthTexture =
			re::Texture::Create(std::format("{}_2DShadow", lightName), shadowParams);

		// Texture target set:
		std::shared_ptr<re::TextureTargetSet> shadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_2DShadowTargetSet", lightName));

		re::TextureTarget::TargetParams depthTargetParams{ .m_textureView = re::TextureView::Texture2DView(0, 1)};
		shadowTargetSet->SetDepthStencilTarget(depthTexture, depthTargetParams);
		shadowTargetSet->SetViewport(re::Viewport(0, 0, depthTexture->Width(), depthTexture->Height()));
		shadowTargetSet->SetScissorRect(
			{ 0, 0, static_cast<long>(depthTexture->Width()), static_cast<long>(depthTexture->Height()) });

		shadowTargetSet->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
			re::TextureTarget::TargetParams::BlendMode::Disabled,
			re::TextureTarget::TargetParams::BlendMode::Disabled });

		shadowTargetSet->SetAllColorWriteModes(re::TextureTarget::TargetParams::ChannelWrite{
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled });

		shadowTargetSet->SetDepthTargetClearMode(re::TextureTarget::TargetParams::ClearMode::Enabled);

		shadowStage->SetTextureTargetSet(shadowTargetSet);

		dstStageData.emplace(
			lightID,
			ShadowStageData{
				.m_renderStage = shadowStage,
				.m_shadowTargetSet = shadowTargetSet,
				.m_shadowCamParamBlock = shadowCamParams });

		m_shadowTextures.emplace(lightID, shadowTargetSet->GetDepthStencilTarget().GetTexture().get());

		SEAssert(m_shadowTextures.size() ==
			(m_directionalShadowStageData.size() +
				m_pointShadowStageData.size() +
				m_spotShadowStageData.size()),
			"Shadow stage data is out of sync with the aggregate map of all shadow textures");
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
				m_shadowTextures.erase(id);
			}
		};
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataDirectional>(), m_directionalShadowStageData);
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataPoint>(), m_pointShadowStageData);
		DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataSpot>(), m_spotShadowStageData);

		SEAssert(m_shadowTextures.size() ==
			(m_directionalShadowStageData.size() +
				m_pointShadowStageData.size() +
				m_spotShadowStageData.size()),
			"Shadow stage data is out of sync with the aggregate map of all shadow textures");

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
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataDirectional>())
		{
			Register2DShadowLights(
				renderData.GetIDsWithNewData<gr::Light::RenderDataDirectional>(), m_directionalShadowStageData);
		}
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataSpot>())
		{
			Register2DShadowLights(renderData.GetIDsWithNewData<gr::Light::RenderDataSpot>(), m_spotShadowStageData);
		}

		// Register new point lights:
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataPoint>())
		{
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
		}


		// Update directional and spot shadow buffers, if necessary:
		auto Update2DShadowCamData = [](
			gr::RenderDataManager::IDIterator const& lightItr,
			std::unordered_map<gr::RenderDataID, ShadowStageData>& stageData,
			bool hasShadow)
			{
				SEAssert(hasShadow == false ||
					(lightItr.HasObjectData<gr::Camera::RenderData>() &&
						lightItr.HasObjectData<gr::ShadowMap::RenderData>()),
					"If a light has a shadow, it must have a shadow camera");

				if (hasShadow &&
					(lightItr.IsDirty<gr::Camera::RenderData>() || lightItr.TransformIsDirty()))
				{
					gr::Camera::RenderData const& shadowCamData = lightItr.Get<gr::Camera::RenderData>();

					stageData.at(lightItr.GetRenderDataID()).m_shadowCamParamBlock->Commit(shadowCamData.m_cameraParams);
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
				Update2DShadowCamData(directionalItr, m_directionalShadowStageData, hasShadow);
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
				Update2DShadowCamData(spotItr, m_spotShadowStageData, hasShadow);
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

				if (hasShadow && 
					(pointItr.IsDirty<gr::Camera::RenderData>() || pointItr.TransformIsDirty()))
				{
					gr::Camera::RenderData const& shadowCamData = pointItr.Get<gr::Camera::RenderData>();
					gr::Transform::RenderData const& transformData = pointItr.GetTransformData();

					CubemapShadowRenderData const& cubemapShadowParams =
						GetCubemapShadowRenderParamsData(shadowCamData, transformData);

					m_pointShadowStageData.at(pointItr.GetRenderDataID()).m_shadowCamParamBlock->Commit(
						cubemapShadowParams);
				}
				++pointItr;
			}
		}

		// Append permanent render stages each frame, to allow dynamic light creation/destruction
		for (auto& directionalStageItr : m_directionalShadowStageData)
		{
			m_stagePipeline->AppendRenderStageForSingleFrame(
				m_directionalParentStageItr, directionalStageItr.second.m_renderStage);
		}
		for (auto& pointStageItr : m_pointShadowStageData)
		{
			m_stagePipeline->AppendRenderStageForSingleFrame(
				m_pointParentStageItr, pointStageItr.second.m_renderStage);
		}
		for (auto& spotStageItr : m_spotShadowStageData)
		{
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