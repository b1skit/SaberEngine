// © 2022 Adam Badke. All rights reserved.
#include "CameraRenderData.h"
#include "Config.h"
#include "GraphicsSystem_Shadows.h"
#include "LightRenderData.h"
#include "RenderManager.h"
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
		, m_hasDirectionalLight(false)
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

		gr::RenderDataManager const& renderDataMgr = m_owningGraphicsSystemManager->GetRenderData();

		// Directional light shadow:		
		m_hasDirectionalLight = !m_renderData[gr::Light::LightType::Directional_Deferred].empty();
		if (m_hasDirectionalLight)
		{
			SEAssert("We currently assume there will only be 1 directional light (even though it's not necessary to)",
				m_renderData[gr::Light::LightType::Directional_Deferred].size() == 1);

			gr::ShadowMap::RenderData const& directionalShadowData = 
				m_renderData[gr::Light::LightType::Directional_Deferred][0];

			gr::Camera::RenderData const& directionalShadowCamRenderData =
				renderDataMgr.GetObjectData<gr::Camera::RenderData>(directionalShadowData.m_renderDataID);
			
			m_directionalShadowCamPB = re::ParameterBlock::Create(
				gr::Camera::CameraParams::s_shaderName,
				directionalShadowCamRenderData.m_cameraParams,
				re::ParameterBlock::PBType::Mutable);

			m_directionalShadowStage->AddPermanentParameterBlock(m_directionalShadowCamPB);

			// Shader:
			m_directionalShadowStage->SetStageShader(
				re::Shader::GetOrCreate(en::ShaderNames::k_depthShaderName, shadowPipelineState));

			char const* lightName = directionalShadowData.m_owningLightName;
			
			// Texture target:
			re::Texture::TextureParams shadowParams;
			shadowParams.m_width = static_cast<uint32_t>(directionalShadowData.m_textureDims.x);
			shadowParams.m_height = static_cast<uint32_t>(directionalShadowData.m_textureDims.y);
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
			// TODO: Target set should be a member of the stage, instead of the shadow map?
			// -> HARD: The stages are already created, we don't know what lights are associated with each stage

			m_shadowTargetSets[gr::Light::LightType::Directional_Deferred].emplace(
				directionalShadowData.m_owningLightID,
				directionalShadowTargetSet);

			pipeline.AppendRenderStage(m_directionalShadowStage);
		}
		
		// Point light shadows:
		const size_t numPointLights = m_renderData[gr::Light::LightType::Point_Deferred].size();

		m_pointLightShadowStages.reserve(numPointLights);
		m_cubemapShadowParamBlocks.reserve(numPointLights);

		for (gr::ShadowMap::RenderData const& shadowData : m_renderData[gr::Light::LightType::Point_Deferred])
		{
			re::RenderStage::GraphicsStageParams gfxStageParams;

			char const* lightName = shadowData.m_owningLightName;
			std::string const& stageName = std::format("{}_Shadow", lightName);
			
			std::shared_ptr<re::RenderStage> shadowStage = re::RenderStage::CreateGraphicsStage(stageName, gfxStageParams);

			shadowStage->SetBatchFilterMaskBit(re::Batch::Filter::NoShadow);
			
			gr::Camera::RenderData const& shadowCamData = 
				renderDataMgr.GetObjectData<gr::Camera::RenderData>(shadowData.m_renderDataID);
			gr::Transform::RenderData const& transformData = renderDataMgr.GetTransformData(shadowData.m_transformID);

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

			m_shadowTargetSets[gr::Light::LightType::Point_Deferred].emplace(
				shadowData.m_owningLightID,
				pointShadowTargetSet);

			shadowStage->SetTextureTargetSet(pointShadowTargetSet);

			// Cubemap shadow param block:
			CubemapShadowRenderParams const& cubemapShadowParams = 
				GetCubemapShadowRenderParamsData(shadowCamData, transformData);

			m_cubemapShadowParamBlocks.emplace_back(re::ParameterBlock::Create(
				CubemapShadowRenderParams::s_shaderName,
				cubemapShadowParams,
				re::ParameterBlock::PBType::Mutable));

			shadowStage->AddPermanentParameterBlock(m_cubemapShadowParamBlocks.back());

			m_pointLightShadowStages.emplace_back(shadowStage);

			pipeline.AppendRenderStage(shadowStage);
		}
	}


	void ShadowsGraphicsSystem::PreRender()
	{
		gr::RenderDataManager const& renderDataMgr = m_owningGraphicsSystemManager->GetRenderData();

		// ECS_CONVERSION: TODO: Detect if the directional shadow has changed (e.g. render data says it's dirty)
		// and upate the PB data.
		// For now, we just force-update it every frame...
		if (m_hasDirectionalLight)
		{
			SEAssert("We currently assume there will only be 1 directional light (even though it's not necessary to)",
				m_renderData[gr::Light::LightType::Directional_Deferred].size() == 1);

			for (gr::ShadowMap::RenderData const& shadowData : m_renderData[gr::Light::LightType::Directional_Deferred])
			{
				gr::Camera::RenderData const& shadowCamData =
					renderDataMgr.GetObjectData<gr::Camera::RenderData>(shadowData.m_renderDataID);

				m_directionalShadowCamPB->Commit(shadowCamData.m_cameraParams);
			}
		}

		for (uint32_t shadowIdx = 0; shadowIdx < m_renderData[gr::Light::LightType::Point_Deferred].size(); shadowIdx++)
		{
			gr::ShadowMap::RenderData const& shadowData = m_renderData[gr::Light::LightType::Point_Deferred][shadowIdx];

			gr::Camera::RenderData const& shadowCamData =
				renderDataMgr.GetObjectData<gr::Camera::RenderData>(shadowData.m_renderDataID);

			gr::Transform::RenderData const& transformData = renderDataMgr.GetTransformData(shadowData.m_transformID);

			// ECS_CONVERSION TODO: Need to maintain a dirty flag here: We're recomputing the CubemapShadowRenderParams
			// every frame -> 6x matrices each!!!!!!!!!!!!!!!!!!
			CubemapShadowRenderParams const& cubemapShadowParams = 
				GetCubemapShadowRenderParamsData(shadowCamData, transformData);

			m_cubemapShadowParamBlocks[shadowIdx]->Commit(cubemapShadowParams);
		}

		CreateBatches();
	}


	void ShadowsGraphicsSystem::CreateBatches()
	{
		// TODO: Create batches specific to this GS: Cached, culled, and with only the appropriate PBs etc attached
		if (m_hasDirectionalLight)
		{
			m_directionalShadowStage->AddBatches(re::RenderManager::Get()->GetSceneBatches());
		}

		for (std::shared_ptr<re::RenderStage> pointShadowStage : m_pointLightShadowStages)
		{
			pointShadowStage->AddBatches(re::RenderManager::Get()->GetSceneBatches());
		}
	}


	std::shared_ptr<re::TextureTargetSet const> ShadowsGraphicsSystem::GetFinalTextureTargetSet() const
	{
		return m_directionalShadowStage->GetTextureTargetSet();
	}


	gr::ShadowMap::RenderData const& ShadowsGraphicsSystem::GetShadowRenderData(
		gr::Light::LightType lightType, gr::LightID lightID) const
	{
		SEAssert("Ambient lights do not have a shadow map", lightType != gr::Light::LightType::AmbientIBL_Deferred);

		auto shadowRenderDataItr = std::find_if(m_renderData[lightType].begin(), m_renderData[lightType].end(),
			[&](gr::ShadowMap::RenderData const& shadowRenderData)
			{
				return shadowRenderData.m_owningLightID == lightID;
			});
		SEAssert("Shadow render data not found", shadowRenderDataItr != m_renderData[lightType].end());

		return *shadowRenderDataItr;
	}


	re::Texture const* ShadowsGraphicsSystem::GetShadowMap(gr::Light::LightType lightType, gr::LightID lightID) const
	{
		SEAssert("Ambient lights do not have a shadow map", lightType != gr::Light::LightType::AmbientIBL_Deferred);
		SEAssert("No shadow data found for the given lightID", m_shadowTargetSets[lightType].contains(lightID));

		return m_shadowTargetSets[lightType].at(lightID)->GetDepthStencilTarget()->GetTexture().get();
	}
}