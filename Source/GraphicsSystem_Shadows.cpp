// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "Config.h"
#include "GraphicsSystem_Culling.h"
#include "GraphicsSystem_Shadows.h"
#include "GraphicsSystemManager.h"
#include "LightRenderData.h"
#include "RenderDataManager.h"
#include "Shader.h"
#include "ShadowMapRenderData.h"

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
}


namespace gr
{
	constexpr char const* k_gsName = "Shadows Graphics System";


	ShadowsGraphicsSystem::ShadowsGraphicsSystem(gr::GraphicsSystemManager* owningGSM)
		: GraphicsSystem(k_gsName, owningGSM)
		, NamedObject(k_gsName)
	{
	}


	void ShadowsGraphicsSystem::CreateRegisterCubeShadowStage(
		std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
		gr::RenderDataID lightID,
		gr::ShadowMap::RenderData const& shadowData,
		gr::Transform::RenderData const& transformData,
		gr::Camera::RenderData const& camData)
	{
		// TODO: FaceCullingMode::Disabled is better for minimizing peter-panning, but we need backface culling if we
		// want to be able to place lights inside of geometry (eg. emissive spheres). For now, enable backface culling.
		// In future, we need to support tagging assets to not cast shadows
		re::PipelineState shadowPipelineState;
		shadowPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);
		shadowPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Less);

		char const* lightName = shadowData.m_owningLightName;
		std::string const& stageName = std::format("{}_Shadow", lightName);

		std::shared_ptr<re::RenderStage> shadowStage =
			re::RenderStage::CreateGraphicsStage(stageName, re::RenderStage::GraphicsStageParams{});

		shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::NoShadow);

		// Shader:
		shadowStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_cubeDepthShaderName, shadowPipelineState));

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
		shadowParams.m_dimension = re::Texture::Dimension::TextureCubeMap;
		shadowParams.m_faces = 6;

		std::shared_ptr<re::Texture> depthTexture = re::Texture::Create(lightName, shadowParams);

		// Texture target set:
		std::shared_ptr<re::TextureTargetSet> pointShadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_ShadowTargetSet", lightName));

		re::TextureTarget::TargetParams depthTargetParams;
		depthTargetParams.m_targetFace = re::TextureTarget::k_allFaces;

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

		pointShadowTargetSet->SetDepthWriteMode(re::TextureTarget::TargetParams::ChannelWrite::Mode::Enabled);
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
	}


	void ShadowsGraphicsSystem::CreateRegister2DShadowStage(
		std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData,
		gr::RenderDataID lightID,
		gr::ShadowMap::RenderData const& shadowData,
		gr::Camera::RenderData const& shadowCamData)
	{
		char const* lightName = shadowData.m_owningLightName;
		std::string const& stageName = std::format("{}_Shadow", lightName);

		std::shared_ptr<re::RenderStage> shadowStage =
			re::RenderStage::CreateGraphicsStage(stageName, re::RenderStage::GraphicsStageParams{});

		shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::NoShadow);

		// Spot shadow camera buffer:
		std::shared_ptr<re::Buffer> shadowCamParams = re::Buffer::Create(
			CameraData::s_shaderName,
			shadowCamData.m_cameraParams,
			re::Buffer::Type::Mutable);

		shadowStage->AddPermanentBuffer(shadowCamParams);

		// Shader:
		// TODO: FaceCullingMode::Disabled is better for minimizing peter-panning, but we need backface culling if we
		// want to be able to place lights inside of geometry (eg. emissive spheres). For now, enable backface culling.
		// In future, we need to support tagging assets to not cast shadows
		re::PipelineState shadowPipelineState;
		shadowPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);
		shadowPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Less);

		shadowStage->SetStageShader(re::Shader::GetOrCreate(en::ShaderNames::k_depthShaderName, shadowPipelineState));

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
		shadowParams.m_faces = 1;

		std::string const& texName = std::format("{}_Shadow", lightName);

		std::shared_ptr<re::Texture> depthTexture = re::Texture::Create(texName, shadowParams);

		// Texture target set:
		std::shared_ptr<re::TextureTargetSet> shadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_ShadowTargetSet", lightName));

		re::TextureTarget::TargetParams depthTargetParams;
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

		shadowTargetSet->SetDepthWriteMode(re::TextureTarget::TargetParams::ChannelWrite::Mode::Enabled);
		shadowTargetSet->SetDepthTargetClearMode(re::TextureTarget::TargetParams::ClearMode::Enabled);

		shadowStage->SetTextureTargetSet(shadowTargetSet);

		dstStageData.emplace(
			lightID,
			ShadowStageData{
				.m_renderStage = shadowStage,
				.m_shadowTargetSet = shadowTargetSet,
				.m_shadowCamParamBlock = shadowCamParams });
	}


	void ShadowsGraphicsSystem::InitPipeline(re::StagePipeline& pipeline)
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


	void ShadowsGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Delete removed deleted lights:
		auto DeleteLights = [](
			std::vector<gr::RenderDataID> const& deletedIDs,
			std::unordered_map<gr::RenderDataID, ShadowStageData>& stageData)
		{
			for (gr::RenderDataID id : deletedIDs)
			{
				stageData.erase(id);
			}
		};
		if (renderData.HasIDsWithDeletedData<gr::Light::RenderDataDirectional>())
		{
			DeleteLights(
				renderData.GetIDsWithDeletedData<gr::Light::RenderDataDirectional>(), m_directionalShadowStageData);
		}
		if (renderData.HasIDsWithDeletedData<gr::Light::RenderDataPoint>())
		{
			DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataPoint>(), m_pointShadowStageData);
		}
		if (renderData.HasIDsWithDeletedData<gr::Light::RenderDataSpot>())
		{
			DeleteLights(renderData.GetIDsWithDeletedData<gr::Light::RenderDataSpot>(), m_spotShadowStageData);
		}

		// Register new directional and spot lights:
		auto Register2DShadowLights = [&](std::vector<gr::RenderDataID> const& newLightIDs,
			std::unordered_map<gr::RenderDataID, ShadowStageData>& dstStageData)
		{
			auto lightItr = renderData.IDBegin(newLightIDs);
			auto const& lightItrEnd = renderData.IDEnd(newLightIDs);
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
			std::vector<gr::RenderDataID> const& newPointIDs = 
				renderData.GetIDsWithNewData<gr::Light::RenderDataPoint>();

			auto pointItr = renderData.IDBegin(newPointIDs);
			auto const& pointItrEnd = renderData.IDEnd(newPointIDs);
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


		CreateBatches();
	}


	void ShadowsGraphicsSystem::CreateBatches()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();
		CullingGraphicsSystem* cullingGS = m_graphicsSystemManager->GetGraphicsSystem<CullingGraphicsSystem>();

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

					directionalStage.AddBatches(m_graphicsSystemManager->GetVisibleBatches(
						gr::Camera::View(lightID, gr::Camera::View::Face::Default),
						gr::BatchManager::InstanceType::Transform));
				}
				++directionalItr;
			}
		}

		if (renderData.HasObjectData<gr::Light::RenderDataSpot>())
		{
			std::vector<gr::RenderDataID> const& spotIDs = cullingGS->GetVisibleSpotLights();

			auto spotItr = renderData.IDBegin(spotIDs);
			auto const& spotItrEnd = renderData.IDEnd(spotIDs);
			while (spotItr != spotItrEnd)
			{
				gr::Light::RenderDataSpot const& spotData = spotItr.Get<gr::Light::RenderDataSpot>();
				if (spotData.m_hasShadow && spotData.m_canContribute)
				{
					const gr::RenderDataID lightID = spotData.m_renderDataID;

					re::RenderStage& spotStage = *m_spotShadowStageData.at(lightID).m_renderStage;

					spotStage.AddBatches(m_graphicsSystemManager->GetVisibleBatches(
						gr::Camera::View(lightID, gr::Camera::View::Face::Default),
						gr::BatchManager::InstanceType::Transform));
				}
				++spotItr;
			}
		}

		if (renderData.HasObjectData<gr::Light::RenderDataPoint>())
		{
			std::vector<gr::RenderDataID> const& pointIDs = cullingGS->GetVisiblePointLights();

			auto pointItr = renderData.IDBegin(pointIDs);
			auto const& pointItrEnd = renderData.IDEnd(pointIDs);
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

					// TODO: We're currently using a geometry shader to project shadows to cubemap faces, so we need to
					// add all batches to the same stage. It might be worth benchmarking performance of moving this to 6
					// individual stages instead
					m_pointShadowStageData.at(pointData.m_renderDataID).m_renderStage->AddBatches(
						m_graphicsSystemManager->GetVisibleBatches(views, gr::BatchManager::InstanceType::Transform));
				}
				++pointItr;
			}
		}
	}


	re::Texture const* ShadowsGraphicsSystem::GetShadowMap(gr::Light::Type lightType, gr::RenderDataID lightID) const
	{
		switch (lightType)
		{
		case gr::Light::Type::Directional:
		{
			SEAssert(m_directionalShadowStageData.contains(lightID), 
				"Light has not been registered for a shadow map, or does not have a shadow map");

			return m_directionalShadowStageData.at(lightID).m_shadowTargetSet->GetDepthStencilTarget()->GetTexture().get();
		}
		break;
		case gr::Light::Type::Point:
		{
			SEAssert(m_pointShadowStageData.contains(lightID),
				"Light has not been registered for a shadow map, or does not have a shadow map");

			return m_pointShadowStageData.at(lightID).m_shadowTargetSet->GetDepthStencilTarget()->GetTexture().get();
		}
		break;
		case gr::Light::Type::Spot:
		{
			SEAssert(m_spotShadowStageData.contains(lightID),
				"Light has not been registered for a shadow map, or does not have a shadow map");

			return m_spotShadowStageData.at(lightID).m_shadowTargetSet->GetDepthStencilTarget()->GetTexture().get();
		}
		break;
		case gr::Light::Type::AmbientIBL:
		default: SEAssertF("Invalid light type, or light type does not support shadow maps");
		}

		return nullptr;
	}
}