// © 2023 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "CameraComponent.h"
#include "Config.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "MarkerComponents.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "RenderManager.h"
#include "ShadowMapComponent.h"
#include "TransformComponent.h"


namespace
{
	gr::Camera::Config ComputeDirectionalShadowCameraConfigFromSceneBounds(
		fr::Transform const& lightTransform, fr::BoundsComponent const& sceneWorldBounds)
	{
		fr::BoundsComponent const& transformedBounds = sceneWorldBounds.GetTransformedAABBBounds(
			glm::inverse(lightTransform.GetGlobalMatrix()));

		gr::Camera::Config shadowCamConfig{};

		shadowCamConfig.m_projectionType = gr::Camera::Config::ProjectionType::Orthographic;

		shadowCamConfig.m_yFOV = 0.f; // Orthographic

		shadowCamConfig.m_near = -transformedBounds.zMax();
		shadowCamConfig.m_far = -transformedBounds.zMin();

		shadowCamConfig.m_orthoLeftRightBotTop.x = transformedBounds.xMin();
		shadowCamConfig.m_orthoLeftRightBotTop.y = transformedBounds.xMax();
		shadowCamConfig.m_orthoLeftRightBotTop.z = transformedBounds.yMin();
		shadowCamConfig.m_orthoLeftRightBotTop.w = transformedBounds.yMax();

		return shadowCamConfig;
	}
}

namespace fr
{
	ShadowMapComponent& ShadowMapComponent::AttachShadowMapComponent(
		EntityManager& em, entt::entity owningEntity, char const* name, fr::Light::Type lightType)
	{
		SEAssert(em.HasComponent<fr::LightComponent>(owningEntity),
			"A ShadowMapComponent must be attached to a LightComponent");
		SEAssert(em.HasComponent<gr::RenderDataComponent>(owningEntity),
			"A ShadowMapComponent must be attached to an entity with a RenderDataComponent");

		// ShadowMap component:
		glm::uvec2 widthHeight{0, 0};
		switch (lightType)
		{
		case fr::Light::Type::Directional:
		{
			SEAssert(em.HasComponent<fr::LightComponent::DirectionalDeferredMarker>(owningEntity),
				"A directional ShadowMapComponent must be attached to an entity with a DirectionalDeferredMarker");

			const int defaultDirectionalWidthHeight = 
				en::Config::Get()->GetValue<int>(en::ConfigKeys::k_defaultShadowMapResolution);
			widthHeight = glm::vec2(defaultDirectionalWidthHeight, defaultDirectionalWidthHeight);
		}
		break;
		case fr::Light::Type::Point:
		{
			SEAssert(em.HasComponent<fr::LightComponent::PointDeferredMarker>(owningEntity),
				"A point ShadowMapComponent must be attached to an entity with a PointDeferredMarker");

			const int defaultCubemapWidthHeight =
				en::Config::Get()->GetValue<int>(en::ConfigKeys::k_defaultShadowCubeMapResolution);
			widthHeight = glm::vec2(defaultCubemapWidthHeight, defaultCubemapWidthHeight);
		}
		break;
		case fr::Light::Type::AmbientIBL:
		default: SEAssertF("Invalid light type");
		}

		fr::LightComponent const& owningLightComponent = em.GetComponent<fr::LightComponent>(owningEntity);
		gr::RenderDataComponent const& sharedRenderDataCmpt = em.GetComponent<gr::RenderDataComponent>(owningEntity);

		ShadowMapComponent& shadowMapComponent = *em.EmplaceComponent<fr::ShadowMapComponent>(
			owningEntity,
			PrivateCTORTag{},
			lightType,
			sharedRenderDataCmpt.GetRenderDataID(),
			sharedRenderDataCmpt.GetTransformID(),
			widthHeight);

		fr::TransformComponent* owningTransform = em.GetFirstInHierarchyAbove<fr::TransformComponent>(owningEntity);
		SEAssert(owningTransform != nullptr, "A shadow map requires a TransformComponent");

		// We need to recompute the Transform, as it's likely dirty during scene construction
		owningTransform->GetTransform().Recompute();

		// Attach a shadow map render camera:
		fr::CameraComponent::AttachCameraComponent(
			em,
			owningEntity,
			std::format("{}_ShadowCam", name).c_str(),
			GenerateShadowCameraConfig(
				shadowMapComponent.GetShadowMap(), 
				owningTransform->GetTransform(), 
				owningLightComponent.GetLight(),
				nullptr));

		// Add a shadow marker:
		em.EmplaceComponent<HasShadowMarker>(owningEntity);

		// Finally, mark our new ShadowMapComponent as dirty:
		em.EmplaceComponent<DirtyMarker<fr::ShadowMapComponent>>(owningEntity);

		return shadowMapComponent;
	}


	gr::Camera::Config ShadowMapComponent::GenerateShadowCameraConfig(
		ShadowMap const& shadowMap, 
		fr::Transform const& lightTransform, 
		fr::Light const& owningLight, 
		fr::BoundsComponent const* sceneWorldBounds)
	{
		gr::Camera::Config shadowCamConfig{};

		switch (shadowMap.GetShadowMapType())
		{
		case fr::ShadowMap::ShadowType::CubeMap:
		{
			SEAssert(owningLight.GetType() == fr::Light::Type::Point, "Unexpected light type");

			shadowCamConfig.m_yFOV = static_cast<float>(std::numbers::pi) * 0.5f;
			shadowCamConfig.m_aspectRatio = 1.0f;
			shadowCamConfig.m_projectionType = gr::Camera::Config::ProjectionType::PerspectiveCubemap;

			constexpr float k_defaultShadowCamNear = 0.1f;
			shadowCamConfig.m_near = k_defaultShadowCamNear;

			shadowCamConfig.m_far = 
				owningLight.GetLightTypeProperties(fr::Light::Type::Point).m_point.m_sphericalRadius;

			// We ignore everything else for shadow map cameras
		}
		break;
		case fr::ShadowMap::ShadowType::Orthographic:
		{
			// Note: We use a zeroed-out bounds as a fallback if the sceneWorldBounds hasn't been created yet
			if (sceneWorldBounds)
			{
				shadowCamConfig = ComputeDirectionalShadowCameraConfigFromSceneBounds(
					lightTransform, *sceneWorldBounds);
			}
			else
			{
				shadowCamConfig = ComputeDirectionalShadowCameraConfigFromSceneBounds(
					lightTransform, fr::BoundsComponent::Zero());
			}
		}
		break;
		default: SEAssertF("Invalid ShadowType");
		}

		return shadowCamConfig;
	}


	gr::ShadowMap::RenderData ShadowMapComponent::CreateRenderData(
		fr::NameComponent const& nameCmpt, fr::ShadowMapComponent const& shadowMapCmpt)
	{
		fr::ShadowMap const& shadowMap = shadowMapCmpt.GetShadowMap();

		gr::ShadowMap::RenderData shadowRenderData = gr::ShadowMap::RenderData
		{
			.m_renderDataID = shadowMapCmpt.GetRenderDataID(),
			.m_transformID = shadowMapCmpt.GetTransformID(),

			.m_lightType = fr::Light::ConvertRenderDataLightType(shadowMap.GetOwningLightType()),
			.m_shadowType = fr::ShadowMap::GetRenderDataShadowMapType(shadowMap.GetShadowMapType()),

			.m_textureDims = re::Texture::ComputeTextureDimenions(shadowMap.GetWidthHeight()),

			.m_minMaxShadowBias = shadowMap.GetMinMaxShadowBias(),
		};

		strncpy(shadowRenderData.m_owningLightName, nameCmpt.GetName().c_str(), en::NamedObject::k_maxNameLength);

		return shadowRenderData;
	}


	bool ShadowMapComponent::Update(
		fr::ShadowMapComponent& shadowMapCmpt,
		fr::TransformComponent const& lightTransformCmpt,
		fr::LightComponent const& lightCmpt,
		fr::CameraComponent& shadowCamCmpt, 
		fr::BoundsComponent const* sceneWorldBounds,
		bool force)
	{
		bool didModify = force;
		if (shadowMapCmpt.GetShadowMap().IsDirty() || force)
		{
			shadowCamCmpt.GetCameraForModification().SetCameraConfig(
				GenerateShadowCameraConfig(
					shadowMapCmpt.GetShadowMap(), 
					lightTransformCmpt.GetTransform(),
					lightCmpt.GetLight(),
					sceneWorldBounds));

			shadowMapCmpt.GetShadowMap().MarkClean();

			didModify = true;
		}
		
		return didModify;
	}


	void ShadowMapComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity shadowMapEntity)
	{
		fr::NameComponent const& nameCmpt = em.GetComponent<fr::NameComponent>(shadowMapEntity);
		if (ImGui::CollapsingHeader(
			std::format("ShadowMap \"{}\"##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			gr::RenderDataComponent::ShowImGuiWindow(em, shadowMapEntity);

			fr::ShadowMapComponent& shadowMapCmpt = em.GetComponent<fr::ShadowMapComponent>(shadowMapEntity);
			shadowMapCmpt.GetShadowMap().ShowImGuiWindow(nameCmpt.GetUniqueID());

			// Shadow camera:
			fr::CameraComponent& shadowCamCmpt = em.GetComponent<fr::CameraComponent>(shadowMapEntity);
			fr::CameraComponent::ShowImGuiWindow(em, shadowMapEntity);

			ImGui::Unindent();
		}
	}


	// ---


	ShadowMapComponent::ShadowMapComponent(
		PrivateCTORTag,
		fr::Light::Type lightType, 
		gr::RenderDataID renderDataID, 
		gr::TransformID transformID,
		glm::uvec2 widthHeight)
		: m_renderDataID(renderDataID)
		, m_transformID(transformID)
		, m_shadowMap(widthHeight, lightType)
	{
		SEAssert(widthHeight.x > 0 && widthHeight.y > 0, "Invalid resolution");
	}


	// ---


	UpdateShadowMapDataRenderCommand::UpdateShadowMapDataRenderCommand(
		fr::NameComponent const& nameCmpt, ShadowMapComponent const& shadowMapCmpt)
		: m_renderDataID(shadowMapCmpt.GetRenderDataID())
		, m_type(fr::Light::ConvertRenderDataLightType(shadowMapCmpt.GetShadowMap().GetOwningLightType()))
		, m_data(fr::ShadowMapComponent::CreateRenderData(nameCmpt, shadowMapCmpt))
	{
	}


	void UpdateShadowMapDataRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		UpdateShadowMapDataRenderCommand* cmdPtr = reinterpret_cast<UpdateShadowMapDataRenderCommand*>(cmdData);

		for (size_t rsIdx = 0; rsIdx < renderSystems.size(); rsIdx++)
		{
			gr::GraphicsSystemManager& gsm = renderSystems[rsIdx]->GetGraphicsSystemManager();

			gr::RenderDataManager& renderDataMgr = gsm.GetRenderDataForModification();

			renderDataMgr.SetObjectData<gr::ShadowMap::RenderData>(cmdPtr->m_renderDataID, &cmdPtr->m_data);
		}
	}


	void UpdateShadowMapDataRenderCommand::Destroy(void* cmdData)
	{
		UpdateShadowMapDataRenderCommand* cmdPtr = reinterpret_cast<UpdateShadowMapDataRenderCommand*>(cmdData);
		cmdPtr->~UpdateShadowMapDataRenderCommand();
	}


	// ---


	DestroyShadowMapDataRenderCommand::DestroyShadowMapDataRenderCommand(ShadowMapComponent const& shadowMapCmpt)
		: m_renderDataID(shadowMapCmpt.GetRenderDataID())
		, m_type(fr::Light::ConvertRenderDataLightType(shadowMapCmpt.GetShadowMap().GetOwningLightType()))
	{
	}


	void DestroyShadowMapDataRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		DestroyShadowMapDataRenderCommand* cmdPtr = reinterpret_cast<DestroyShadowMapDataRenderCommand*>(cmdData);

		for (size_t rsIdx = 0; rsIdx < renderSystems.size(); rsIdx++)
		{
			gr::GraphicsSystemManager& gsm = renderSystems[rsIdx]->GetGraphicsSystemManager();

			gr::RenderDataManager& renderDataMgr = gsm.GetRenderDataForModification();

			renderDataMgr.DestroyObjectData<gr::ShadowMap::RenderData>(cmdPtr->m_renderDataID);
		}
	}


	void DestroyShadowMapDataRenderCommand::Destroy(void* cmdData)
	{
		DestroyShadowMapDataRenderCommand* cmdPtr = reinterpret_cast<DestroyShadowMapDataRenderCommand*>(cmdData);
		cmdPtr->~DestroyShadowMapDataRenderCommand();
	}
}