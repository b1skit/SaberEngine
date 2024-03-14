// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "Config.h"
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


	std::shared_ptr<re::RenderStage> ShadowsGraphicsSystem::CreateRegisterDirectionalShadowStage(
		gr::RenderDataID lightID,
		gr::ShadowMap::RenderData const& shadowData,
		gr::Camera::RenderData const& shadowCamData)
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

		// Directional shadow camera buffer:
		std::shared_ptr<re::Buffer> shadowCamParams = re::Buffer::Create(
			CameraData::s_shaderName,
			shadowCamData.m_cameraParams,
			re::Buffer::Type::Mutable);

		shadowStage->AddPermanentBuffer(shadowCamParams);

		// Shader:
		shadowStage->SetStageShader(
			re::Shader::GetOrCreate(en::ShaderNames::k_depthShaderName, shadowPipelineState));

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
		std::shared_ptr<re::TextureTargetSet> directionalShadowTargetSet =
			re::TextureTargetSet::Create(std::format("{}_ShadowTargetSet", lightName));

		re::TextureTarget::TargetParams depthTargetParams;
		directionalShadowTargetSet->SetDepthStencilTarget(depthTexture, depthTargetParams);
		directionalShadowTargetSet->SetViewport(re::Viewport(0, 0, depthTexture->Width(), depthTexture->Height()));
		directionalShadowTargetSet->SetScissorRect(
			{ 0, 0, static_cast<long>(depthTexture->Width()), static_cast<long>(depthTexture->Height()) });

		directionalShadowTargetSet->SetAllColorTargetBlendModes(re::TextureTarget::TargetParams::BlendModes{
			re::TextureTarget::TargetParams::BlendMode::Disabled,
			re::TextureTarget::TargetParams::BlendMode::Disabled });

		directionalShadowTargetSet->SetAllColorWriteModes(re::TextureTarget::TargetParams::ChannelWrite{
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled,
			re::TextureTarget::TargetParams::ChannelWrite::Mode::Disabled });

		directionalShadowTargetSet->SetDepthWriteMode(re::TextureTarget::TargetParams::ChannelWrite::Mode::Enabled);
		directionalShadowTargetSet->SetDepthTargetClearMode(re::TextureTarget::TargetParams::ClearMode::Enabled);

		shadowStage->SetTextureTargetSet(directionalShadowTargetSet);

		m_directionalShadowStageData.emplace(
			lightID,
			DirectionalShadowStageData{
				.m_renderStage = shadowStage,
				.m_shadowTargetSet = directionalShadowTargetSet,
				.m_shadowCamParamBlock = shadowCamParams });

		return shadowStage;
	}


	std::shared_ptr<re::RenderStage> ShadowsGraphicsSystem::CreateRegisterPointShadowStage(
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
		CubemapShadowRenderData const& cubemapShadowParams =
			GetCubemapShadowRenderParamsData(camData, transformData);

		std::shared_ptr<re::Buffer> cubeShadowBuf = re::Buffer::Create(
			CubemapShadowRenderData::s_shaderName,
			cubemapShadowParams,
			re::Buffer::Type::Mutable);

		shadowStage->AddPermanentBuffer(cubeShadowBuf);

		m_pointShadowStageData.emplace(
			lightID,
			PointShadowStageData{
				.m_renderStage = shadowStage,
				.m_shadowTargetSet = pointShadowTargetSet,
				.m_cubemapShadowParamBlock = cubeShadowBuf });

		return shadowStage;
	}


	void ShadowsGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		m_stagePipeline = &pipeline;

		std::shared_ptr<re::RenderStage> directionalParentStage = 
			re::RenderStage::CreateParentStage("Directional shadow stages");
		m_directionalParentStageItr = pipeline.AppendRenderStage(directionalParentStage);

		std::shared_ptr<re::RenderStage> pointParentStage = re::RenderStage::CreateParentStage("Point shadow stages");
		m_pointParentStageItr = pipeline.AppendRenderStage(pointParentStage);
	}


	void ShadowsGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Delete removed deleted lights:
		auto DeleteLights = []<typename T>(
			std::vector<gr::RenderDataID> const& deletedIDs, std::unordered_map<gr::RenderDataID, T>&stageData)
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


		// Register new directional lights:
		if (renderData.HasIDsWithNewData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> const& newDirectionalIDs = 
				renderData.GetIDsWithNewData<gr::Light::RenderDataDirectional>();

			auto directionalItr = renderData.IDBegin(newDirectionalIDs);
			auto const& directionalItrEnd = renderData.IDEnd(newDirectionalIDs);
			while (directionalItr != directionalItrEnd)
			{
				if (directionalItr.HasObjectData<gr::ShadowMap::RenderData>() == true)
				{
					SEAssert(directionalItr.HasObjectData<gr::Camera::RenderData>(),
						"Shadow map and shadow camera render data are both required for shadows");

					CreateRegisterDirectionalShadowStage(
						directionalItr.GetRenderDataID(),
						directionalItr.Get<gr::ShadowMap::RenderData>(),
						directionalItr.Get<gr::Camera::RenderData>());
				}
				++directionalItr;
			}
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

					CreateRegisterPointShadowStage(
						pointItr.GetRenderDataID(),
						pointItr.Get<gr::ShadowMap::RenderData>(),
						pointItr.GetTransformData(),
						pointItr.Get<gr::Camera::RenderData>());
				}
				++pointItr;
			}
		}

		// Update directional shadow param blocks, if necessary: 
		if (renderData.HasObjectData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> directionalIDs = 
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataDirectional>();

			auto directionalItr = renderData.IDBegin(directionalIDs);
			auto const& directionalItrEnd = renderData.IDEnd(directionalIDs);
			while (directionalItr != directionalItrEnd)
			{
				const bool hasShadow = directionalItr.Get<gr::Light::RenderDataDirectional>().m_hasShadow;

				SEAssert(hasShadow == false ||
					(directionalItr.HasObjectData<gr::Camera::RenderData>()&&
						directionalItr.HasObjectData<gr::ShadowMap::RenderData>()),
					"If a light has a shadow, it must have a shadow camera");
			
				if (hasShadow &&
					(directionalItr.IsDirty<gr::Camera::RenderData>() || directionalItr.TransformIsDirty()))
				{
					gr::Camera::RenderData const& shadowCamData = directionalItr.Get<gr::Camera::RenderData>();

					m_directionalShadowStageData.at(directionalItr.GetRenderDataID()).m_shadowCamParamBlock->Commit(
						shadowCamData.m_cameraParams);
				}
				++directionalItr;
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

					m_pointShadowStageData.at(pointItr.GetRenderDataID()).m_cubemapShadowParamBlock->Commit(
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
				if (directionalData.m_hasShadow && directionalData.m_colorIntensity.w > 0.f)
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

		if (renderData.HasObjectData<gr::Light::RenderDataPoint>())
		{
			std::vector<gr::RenderDataID> pointIDs = renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataPoint>();
			auto pointItr = renderData.IDBegin(pointIDs);
			auto const& pointItrEnd = renderData.IDEnd(pointIDs);
			while (pointItr != pointItrEnd)
			{
				gr::Light::RenderDataPoint const& pointData = pointItr.Get<gr::Light::RenderDataPoint>();
				if (pointData.m_hasShadow && pointData.m_colorIntensity.w > 0.f)
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
		case gr::Light::Type::AmbientIBL:
		default: SEAssertF("Invalid light type, or light type does not support shadow maps");
		}

		return nullptr;
	}
}