// © 2023 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "CameraComponent.h"
#include "Config.h"
#include "EntityManager.h"
#include "GraphicsSystem_Shadows.h"
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
		EntityManager& em, entt::entity owningEntity, char const* name, fr::Light::LightType lightType)
	{
		SEAssert("A ShadowMapComponent must be attached to a LightComponent",
			em.HasComponent<fr::LightComponent>(owningEntity));

		entt::entity shadowMapEntity = em.CreateEntity(name);

		// Relationship:
		fr::Relationship& shadowMapRelationship = em.GetComponent<fr::Relationship>(shadowMapEntity);
		shadowMapRelationship.SetParent(em, owningEntity);

		// RenderData: We share the owning entity's RenderDataID
		gr::RenderDataComponent* owningRenderDataCmpt = 
			em.GetFirstInHierarchyAbove<gr::RenderDataComponent>(shadowMapRelationship.GetParent());
		SEAssert("A shadow map needs to share a render data component", owningRenderDataCmpt != nullptr);

		gr::RenderDataComponent const& sharedRenderDataCmpt = 
			gr::RenderDataComponent::AttachSharedRenderDataComponent(em, shadowMapEntity, *owningRenderDataCmpt);

		// ShadowMap component:
		glm::uvec2 widthHeight{0, 0};
		switch (lightType)
		{
		case fr::Light::LightType::Directional_Deferred:
		{
			const int defaultDirectionalWidthHeight = 
				en::Config::Get()->GetValue<int>(en::ConfigKeys::k_defaultShadowMapResolution);
			widthHeight = glm::vec2(defaultDirectionalWidthHeight, defaultDirectionalWidthHeight);

			// Add a light type marker to simplify shadow searches:
			em.EmplaceComponent<fr::LightComponent::DirectionalDeferredMarker>(shadowMapEntity);
		}
		break;
		case fr::Light::LightType::Point_Deferred:
		{
			const int defaultCubemapWidthHeight =
				en::Config::Get()->GetValue<int>(en::ConfigKeys::k_defaultShadowCubeMapResolution);
			widthHeight = glm::vec2(defaultCubemapWidthHeight, defaultCubemapWidthHeight);
			
			em.EmplaceComponent<fr::LightComponent::PointDeferredMarker>(shadowMapEntity);
		}
		break;
		case fr::Light::LightType::AmbientIBL_Deferred:
		default: SEAssertF("Invalid light type");
		}

		fr::LightComponent const& owningLightComponent = em.GetComponent<fr::LightComponent>(owningEntity);

		ShadowMapComponent& shadowMapComponent = *em.EmplaceComponent<fr::ShadowMapComponent>(
			shadowMapEntity,
			PrivateCTORTag{},
			owningLightComponent.GetLightID(),
			lightType,
			sharedRenderDataCmpt.GetRenderDataID(),
			sharedRenderDataCmpt.GetTransformID(),
			widthHeight);

		fr::TransformComponent* owningTransform = em.GetFirstInHierarchyAbove<fr::TransformComponent>(owningEntity);
		SEAssert("A shadow map requires a TransformComponent", owningTransform != nullptr);

		// We need to recompute the Transform, as it's likely dirty during scene construction
		owningTransform->GetTransform().Recompute();

		// Attach a shadow map render camera:
		fr::CameraComponent::AttachCameraConcept(
			em,
			shadowMapEntity,
			std::format("{}_ShadowCam", name).c_str(),
			GenerateShadowCameraConfig(
				shadowMapComponent.GetShadowMap(), 
				owningTransform->GetTransform(), 
				owningLightComponent.GetLight(),
				nullptr));

		// Finally, mark our new ShadowMapComponent as dirty:
		em.EmplaceComponent<DirtyMarker<fr::ShadowMapComponent>>(shadowMapEntity);

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
			SEAssert("Unexpected light type", owningLight.GetType() == fr::Light::LightType::Point_Deferred);

			shadowCamConfig.m_yFOV = static_cast<float>(std::numbers::pi) * 0.5f;
			shadowCamConfig.m_aspectRatio = 1.0f;
			shadowCamConfig.m_projectionType = gr::Camera::Config::ProjectionType::PerspectiveCubemap;

			constexpr float k_defaultShadowCamNear = 0.1f;
			shadowCamConfig.m_near = k_defaultShadowCamNear;

			shadowCamConfig.m_far = 
				owningLight.GetLightTypeProperties(fr::Light::LightType::Point_Deferred).m_point.m_sphericalRadius;

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
			.m_owningLightID = shadowMapCmpt.GetLightID(),
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
			std::format("{}##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			gr::RenderDataComponent::ShowImGuiWindow(em, shadowMapEntity);

			fr::ShadowMapComponent& shadowMapCmpt = em.GetComponent<fr::ShadowMapComponent>(shadowMapEntity);

			shadowMapCmpt.GetShadowMap().ShowImGuiWindow(nameCmpt.GetUniqueID());

			// Shadow camera:
			{
				entt::entity shadowCamEntity = entt::null;
				fr::CameraComponent* shadowCamCmpt = em.GetFirstAndEntityInChildren<fr::CameraComponent>(shadowMapEntity, shadowCamEntity);
				SEAssert("Failed to find shadow camera", shadowCamCmpt);

				fr::CameraComponent::ShowImGuiWindow(em, shadowCamEntity);
			}

			ImGui::Unindent();
		}
	}


	// ---


	ShadowMapComponent::ShadowMapComponent(
		PrivateCTORTag,
		gr::LightID lightID, 
		fr::Light::LightType lightType, 
		gr::RenderDataID renderDataID, 
		gr::TransformID transformID,
		glm::uvec2 widthHeight)
		: m_lightID(lightID)
		, m_renderDataID(renderDataID)
		, m_transformID(transformID)
		, m_shadowMap(widthHeight, lightType)
	{
		SEAssert("Invalid resolution", widthHeight.x > 0 && widthHeight.y > 0);
	}


	// ---


	UpdateShadowMapDataRenderCommand::UpdateShadowMapDataRenderCommand(
		fr::NameComponent const& nameCmpt, ShadowMapComponent const& shadowMapCmpt)
		: m_lightID(shadowMapCmpt.GetLightID())
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
			gr::ShadowsGraphicsSystem* shadowGS =
				renderSystems[rsIdx]->GetGraphicsSystemManager().GetGraphicsSystem<gr::ShadowsGraphicsSystem>();

			if (shadowGS)
			{
				gr::ShadowMap::RenderData const& shadowMapRenderData = cmdPtr->m_data;

				std::vector<gr::ShadowMap::RenderData>* gsRenderData = nullptr;

				switch (shadowMapRenderData.m_lightType)
				{
				case gr::Light::LightType::AmbientIBL_Deferred:
				{
					gsRenderData = &shadowGS->GetRenderData(gr::Light::LightType::AmbientIBL_Deferred);
				}
				break;
				case gr::Light::LightType::Directional_Deferred:
				{
					gsRenderData = &shadowGS->GetRenderData(gr::Light::LightType::Directional_Deferred);
				}
				break;
				case gr::Light::LightType::Point_Deferred:
				{
					gsRenderData = &shadowGS->GetRenderData(gr::Light::LightType::Point_Deferred);
				}
				break;
				default: SEAssertF("Invalid light type");
				}

				auto existingLightItr = std::find_if(gsRenderData->begin(), gsRenderData->end(),
					[&](gr::ShadowMap::RenderData const& existingShadowMap)
					{
						return shadowMapRenderData.m_owningLightID == existingShadowMap.m_owningLightID;
					});

				if (existingLightItr == gsRenderData->end()) // New light
				{
					gsRenderData->emplace_back(shadowMapRenderData);
				}
				else
				{
					*existingLightItr = shadowMapRenderData;
				}
			}
		}
	}


	void UpdateShadowMapDataRenderCommand::Destroy(void* cmdData)
	{
		UpdateShadowMapDataRenderCommand* cmdPtr = reinterpret_cast<UpdateShadowMapDataRenderCommand*>(cmdData);
		cmdPtr->~UpdateShadowMapDataRenderCommand();
	}
}