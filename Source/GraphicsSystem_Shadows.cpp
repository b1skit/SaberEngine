// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "Config.h"
#include "GraphicsSystem_Shadows.h"
#include "LightRenderData.h"
#include "RenderManager.h"
#include "Shader.h"
#include "ShadowMapRenderData.h"


namespace
{
	struct CubemapShadowRenderParams
	{
		glm::mat4 g_cubemapShadowCam_VP[6];
		glm::vec4 g_cubemapShadowCamNearFar; // .xy = near, far. .zw = unused
		glm::vec4 g_cubemapLightWorldPos; // .xyz = light word pos, .w = unused

		static constexpr char const* const s_shaderName = "CubemapShadowRenderParams"; // Not counted towards size of struct
	};


	CubemapShadowRenderParams GetCubemapShadowRenderParamsData(
		gr::Camera::RenderData const& shadowCamData, gr::Transform::RenderData const& transformData)
	{
		SEAssert("Invalid projection type",
			shadowCamData.m_cameraConfig.m_projectionType == gr::Camera::Config::ProjectionType::PerspectiveCubemap);

		CubemapShadowRenderParams cubemapShadowParams;
		
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
		re::RenderStage::GraphicsStageParams gfxStageParams;
		m_directionalShadowStage = re::RenderStage::CreateGraphicsStage("Directional shadow", gfxStageParams);

		m_directionalShadowStage->SetBatchFilterMaskBit(re::Batch::Filter::NoShadow);
	}


	void ShadowsGraphicsSystem::Create(re::StagePipeline& pipeline)
	{
		re::PipelineState shadowPipelineState;
		
		// TODO: FaceCullingMode::Disabled is better for minimizing peter-panning, but we need backface culling if we
		// want to be able to place lights inside of geometry (eg. emissive spheres). For now, enable backface culling.
		// In future, we need to support tagging assets to not cast shadows
		shadowPipelineState.SetFaceCullingMode(re::PipelineState::FaceCullingMode::Back);
		shadowPipelineState.SetDepthTestMode(re::PipelineState::DepthTestMode::Less);

		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// Directional light shadow:
		if (renderData.HasObjectData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> const& directionalIDs =
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataDirectional>();

			SEAssert("We currently assume there will only be 1 directional light (even though it's not necessary to)",
				directionalIDs.size() == 1);

			auto directionalItr = renderData.IDBegin(directionalIDs);
			auto const& directionalItrEnd = renderData.IDEnd(directionalIDs);
			while (directionalItr != directionalItrEnd)
			{
				gr::Light::RenderDataDirectional const& directionalData =
					directionalItr.Get<gr::Light::RenderDataDirectional>();
				if (directionalData.m_hasShadow)
				{
					gr::Camera::RenderData const& camData = directionalItr.Get<gr::Camera::RenderData>();

					m_directionalShadowCamPB = re::ParameterBlock::Create(
						gr::Camera::CameraParams::s_shaderName,
						camData.m_cameraParams,
						re::ParameterBlock::PBType::Mutable);

					m_directionalShadowStage->AddPermanentParameterBlock(m_directionalShadowCamPB);

					// Shader:
					m_directionalShadowStage->SetStageShader(
						re::Shader::GetOrCreate(en::ShaderNames::k_depthShaderName, shadowPipelineState));

					gr::ShadowMap::RenderData const& shadowData = directionalItr.Get<gr::ShadowMap::RenderData>();

					char const* lightName = shadowData.m_owningLightName;

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
					// 2D:
					shadowParams.m_dimension = re::Texture::Dimension::Texture2D;
					shadowParams.m_faces = 1;

					std::string const& texName = std::format("{}_Shadow", lightName);

					std::shared_ptr<re::Texture> depthTexture = re::Texture::Create(texName, shadowParams, false);

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

					m_directionalShadowStage->SetTextureTargetSet(directionalShadowTargetSet);

					m_shadowTargetSets[gr::Light::Type::Directional].emplace(
						shadowData.m_renderDataID,
						directionalShadowTargetSet);

					pipeline.AppendRenderStage(m_directionalShadowStage);
				}

				++directionalItr;
			}
		}
		
		// Point light shadows:
		const size_t numPointLights = renderData.GetNumElementsOfType<gr::Light::RenderDataPoint>();
		if (numPointLights > 0)
		{
			m_pointLightStageData.reserve(numPointLights);

			std::vector<gr::RenderDataID> const& pointIDs =
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataPoint>();

			auto pointItr = renderData.IDBegin(pointIDs);
			auto const& pointItrEnd = renderData.IDEnd(pointIDs);
			while (pointItr != pointItrEnd)
			{
				gr::Light::RenderDataPoint const& pointData = pointItr.Get<gr::Light::RenderDataPoint>();
				if (pointData.m_hasShadow)
				{
					re::RenderStage::GraphicsStageParams gfxStageParams;

					gr::ShadowMap::RenderData const& pointData = pointItr.Get<gr::ShadowMap::RenderData>();

					char const* lightName = pointData.m_owningLightName;
					std::string const& stageName = std::format("{}_Shadow", lightName);

					std::shared_ptr<re::RenderStage> shadowStage = 
						re::RenderStage::CreateGraphicsStage(stageName, gfxStageParams);

					shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::NoShadow);

					gr::Camera::RenderData const& shadowCamData = pointItr.Get<gr::Camera::RenderData>();
					gr::Transform::RenderData const& transformData = pointItr.GetTransformDataFromTransformID();

					// Shader:
					shadowStage->SetStageShader(
						re::Shader::GetOrCreate(en::ShaderNames::k_cubeDepthShaderName, shadowPipelineState));

					// Texture target:
					re::Texture::TextureParams shadowParams;
					shadowParams.m_width = static_cast<uint32_t>(pointData.m_textureDims.x);
					shadowParams.m_height = static_cast<uint32_t>(pointData.m_textureDims.y);
					shadowParams.m_usage =
						static_cast<re::Texture::Usage>(re::Texture::Usage::DepthTarget | re::Texture::Usage::Color);
					shadowParams.m_format = re::Texture::Format::Depth32F;
					shadowParams.m_colorSpace = re::Texture::ColorSpace::Linear;
					shadowParams.m_mipMode = re::Texture::MipMode::None;
					shadowParams.m_addToSceneData = false;
					shadowParams.m_clear.m_depthStencil.m_depth = 1.f;
					// Cubemap:
					shadowParams.m_dimension = re::Texture::Dimension::TextureCubeMap;
					shadowParams.m_faces = 6;

					std::shared_ptr<re::Texture> depthTexture = re::Texture::Create(lightName, shadowParams, false);

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

					m_shadowTargetSets[gr::Light::Type::Point].emplace(
						pointData.m_renderDataID,
						pointShadowTargetSet);

					shadowStage->SetTextureTargetSet(pointShadowTargetSet);

					// Cubemap shadow param block:
					CubemapShadowRenderParams const& cubemapShadowParams =
						GetCubemapShadowRenderParamsData(shadowCamData, transformData);

					std::shared_ptr<re::ParameterBlock> cubeShadowPB = re::ParameterBlock::Create(
						CubemapShadowRenderParams::s_shaderName,
						cubemapShadowParams,
						re::ParameterBlock::PBType::Mutable);

					shadowStage->AddPermanentParameterBlock(cubeShadowPB);

					pipeline.AppendRenderStage(shadowStage);

					m_pointLightStageData.emplace(
						pointItr.GetRenderDataID(),
						PointLightStageData{
							.m_renderStage = shadowStage,
							.m_cubemapShadowParamBlock = cubeShadowPB });
				}

				++pointItr;
			}
		}
	}


	void ShadowsGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		if (renderData.HasObjectData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> directionalIDs = 
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataDirectional>();

			SEAssert("We currently assume there will only be 1 directional light (even though it's not necessary to)",
				directionalIDs.size() == 1);

			auto directionalItr = renderData.IDBegin(directionalIDs);
			auto const& directionalItrEnd = renderData.IDEnd(directionalIDs);
			while (directionalItr != directionalItrEnd)
			{
				if (directionalItr.IsDirty<gr::Camera::RenderData>())
				{
					gr::Camera::RenderData const& shadowCamData = directionalItr.Get<gr::Camera::RenderData>();
					m_directionalShadowCamPB->Commit(shadowCamData.m_cameraParams);
				}
				++directionalItr;
			}
		}

		if (renderData.HasObjectData<gr::Light::RenderDataPoint>())
		{
			std::vector<gr::RenderDataID> const& pointIDs =
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataPoint>();
			auto pointItr = renderData.IDBegin(pointIDs);
			auto const& pointItrEnd = renderData.IDEnd(pointIDs);
			while (pointItr != pointItrEnd)
			{
				if (pointItr.IsDirty<gr::Camera::RenderData>() || pointItr.TransformIsDirty())
				{
					gr::Camera::RenderData const& shadowCamData = pointItr.Get<gr::Camera::RenderData>();
					gr::Transform::RenderData const& transformData = pointItr.GetTransformDataFromTransformID();

					CubemapShadowRenderParams const& cubemapShadowParams =
						GetCubemapShadowRenderParamsData(shadowCamData, transformData);

					m_pointLightStageData.at(pointItr.GetRenderDataID()).m_cubemapShadowParamBlock->Commit(cubemapShadowParams);
				}

				++pointItr;
			}
		}

		CreateBatches();
	}


	void ShadowsGraphicsSystem::CreateBatches()
	{
		gr::RenderDataManager const& renderData = m_graphicsSystemManager->GetRenderData();

		// TODO: Create batches specific to this GS: Cached, culled, and with only the appropriate PBs etc attached
		if (renderData.HasObjectData<gr::Light::RenderDataDirectional>())
		{
			std::vector<gr::RenderDataID> directionalIDs =
				renderData.GetRegisteredRenderDataIDs<gr::Light::RenderDataDirectional>();

			SEAssert("We currently assume there will only be 1 directional light (even though it's not necessary to)",
				directionalIDs.size() == 1);

			auto directionalItr = renderData.IDBegin(directionalIDs);
			auto const& directionalItrEnd = renderData.IDEnd(directionalIDs);
			while (directionalItr != directionalItrEnd)
			{
				gr::Light::RenderDataDirectional const& directionalData = 
					directionalItr.Get<gr::Light::RenderDataDirectional>();
				if (directionalData.m_hasShadow && directionalData.m_colorIntensity.w > 0.f)
				{
					m_directionalShadowStage->AddBatches(re::RenderManager::Get()->GetSceneBatches());
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
					m_pointLightStageData.at(pointItr.GetRenderDataID()).m_renderStage->AddBatches(
						re::RenderManager::Get()->GetSceneBatches());
				}

				++pointItr;
			}
		}
	}


	std::shared_ptr<re::TextureTargetSet const> ShadowsGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_directionalShadowStage->GetTextureTargetSet();
	}


	re::Texture const* ShadowsGraphicsSystem::GetShadowMap(gr::Light::Type lightType, gr::RenderDataID lightID) const
	{
		SEAssert("Ambient lights do not have a shadow map", lightType != gr::Light::Type::AmbientIBL);
		SEAssert("No shadow data found for the given lightID", m_shadowTargetSets[lightType].contains(lightID));

		return m_shadowTargetSets[lightType].at(lightID)->GetDepthStencilTarget()->GetTexture().get();
	}
}