// © 2023 Adam Badke. All rights reserved.
#include "CameraComponent.h"
#include "Config.h"
#include "GameplayManager.h"
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
		fr::Transform& lightTransform, fr::BoundsComponent const& sceneWorldBounds)
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
		GameplayManager& gpm, entt::entity owningEntity, char const* name, fr::Light::LightType lightType)
	{
		SEAssert("A ShadowMapComponent must be attached to a LightComponent",
			gpm.HasComponent<fr::LightComponent>(owningEntity));

		entt::entity shadowMapEntity = gpm.CreateEntity(name);

		// Relationship:
		fr::Relationship& shadowMapRelationship = gpm.GetComponent<fr::Relationship>(shadowMapEntity);
		shadowMapRelationship.SetParent(gpm, owningEntity);

		// RenderData: We share the owning entity's RenderDataID
		gr::RenderDataComponent* owningRenderDataCmpt = 
			gpm.GetFirstInHierarchyAbove<gr::RenderDataComponent>(shadowMapRelationship.GetParent());
		SEAssert("A shadow map needs to share a render data component", owningRenderDataCmpt != nullptr);

		gr::RenderDataComponent const& sharedRenderDataCmpt = 
			gr::RenderDataComponent::AttachSharedRenderDataComponent(gpm, shadowMapEntity, *owningRenderDataCmpt);

		SEAssert("A shadow map requires a TransformComponent",
			gpm.IsInHierarchyAbove<fr::TransformComponent>(owningEntity));

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
			gpm.EmplaceComponent<fr::LightComponent::DirectionalDeferredMarker>(shadowMapEntity);
		}
		break;
		case fr::Light::LightType::Point_Deferred:
		{
			const int defaultCubemapWidthHeight =
				en::Config::Get()->GetValue<int>(en::ConfigKeys::k_defaultShadowCubeMapResolution);
			widthHeight = glm::vec2(defaultCubemapWidthHeight, defaultCubemapWidthHeight);
			
			gpm.EmplaceComponent<fr::LightComponent::PointDeferredMarker>(shadowMapEntity);
		}
		break;
		case fr::Light::LightType::AmbientIBL_Deferred:
		default: SEAssertF("Invalid light type");
		}

		fr::LightComponent const& owningLightComponent = gpm.GetComponent<fr::LightComponent>(owningEntity);

		ShadowMapComponent& shadowMapComponent = *gpm.EmplaceComponent<fr::ShadowMapComponent>(
			shadowMapEntity,
			PrivateCTORTag{},
			owningLightComponent.GetLightID(),
			lightType,
			sharedRenderDataCmpt.GetRenderDataID(),
			sharedRenderDataCmpt.GetTransformID(),
			widthHeight);

		// Mark our new ShadowMapComponent as dirty:
		gpm.EmplaceComponent<DirtyMarker<fr::ShadowMapComponent>>(shadowMapEntity);

		// Attach a shadow map render camera:
		fr::CameraComponent::AttachCameraComponent(
			gpm,
			shadowMapEntity,
			std::format("{}_ShadowCam", name).c_str(),
			GenerateShadowCameraConfig(gpm, shadowMapEntity, shadowMapComponent));

		return shadowMapComponent;
	}


	gr::Camera::Config ShadowMapComponent::GenerateShadowCameraConfig(
		fr::GameplayManager& gpm, entt::entity shadowMapEntity, ShadowMapComponent const& shadowMapCmpt)
	{
		fr::ShadowMap const& shadowMap = shadowMapCmpt.GetShadowMap();

		gr::Camera::Config shadowCamConfig{};

		switch (shadowMap.GetShadowMapType())
		{
		case fr::ShadowMap::ShadowType::CubeMap:
		{
			shadowCamConfig.m_yFOV = static_cast<float>(std::numbers::pi) * 0.5f;
			shadowCamConfig.m_near = 0.1f; // ECS_CONVERSION TODO: TEMP HAX: This should be computed in the GS from the point light radius!!!!!!!!!!
			shadowCamConfig.m_far = 50.f; // ECS_CONVERSION TODO: TEMP HAX: This should be computed in the GS from the point light radius!!!!!!!!!!
			shadowCamConfig.m_aspectRatio = 1.0f;
			shadowCamConfig.m_projectionType = gr::Camera::Config::ProjectionType::PerspectiveCubemap;

			// We ignore everything else for shadow map cameras
		}
		break;
		case fr::ShadowMap::ShadowType::Orthographic:
		{
			// ECS_CONVERSION: TODO: CLEAN THIS UP!!!!!!!!!!!!!!!!
			// -> THIS IS A HOT MESS WITH SIDE EFFECTS (e.g. we're accessing the Transform to get the global matrix,
			// which could cause a recomputation)
			// 
			// Need to be really careful with threading here: We're getting bounds and transforms, but they could (eventually)
			// be updated on other threads when we're updating lights/shadows on another? 
			//	-> LEAVE COMMENTS IN GameplayManager once this is all working...
			
			fr::TransformComponent* transformComponent =
				gpm.GetFirstInHierarchyAbove<fr::TransformComponent>(shadowMapEntity);
			SEAssert("Cannot find TransformComponent", transformComponent != nullptr);

			// Update shadow cam bounds:
			fr::BoundsComponent const* sceneWorldBounds = gpm.GetSceneBounds();
			if (sceneWorldBounds)
			{
				shadowCamConfig = ComputeDirectionalShadowCameraConfigFromSceneBounds(
					transformComponent->GetTransform(), *sceneWorldBounds);
			}
			else
			{
				shadowCamConfig = ComputeDirectionalShadowCameraConfigFromSceneBounds(
					transformComponent->GetTransform(), fr::BoundsComponent::Zero());
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

			.m_lightType = fr::Light::GetRenderDataLightType(shadowMap.GetOwningLightType()),
			.m_shadowType = fr::ShadowMap::GetRenderDataShadowMapType(shadowMap.GetShadowMapType()),

			.m_textureDims = re::Texture::ComputeTextureDimenions(shadowMap.GetWidthHeight()),

			.m_minMaxShadowBias = shadowMap.GetMinMaxShadowBias(),
		};

		strncpy(shadowRenderData.m_owningLightName, nameCmpt.GetName().c_str(), en::NamedObject::k_maxNameLength);

		return shadowRenderData;
	}


	void ShadowMapComponent::MarkDirty(GameplayManager& gpm, entt::entity shadowMapEntity)
	{
		gpm.TryEmplaceComponent<DirtyMarker<fr::ShadowMapComponent>>(shadowMapEntity);

		entt::entity cameraEntity = entt::null;
		fr::CameraComponent* shadowCamCmpt = 
			gpm.GetFirstInChildren<fr::CameraComponent>(shadowMapEntity, cameraEntity);
		SEAssert("Could not find shadow camera", shadowCamCmpt);

		fr::ShadowMapComponent const& shadowMapCmpt = gpm.GetComponent<fr::ShadowMapComponent>(shadowMapEntity);
		shadowCamCmpt->GetCamera().SetCameraConfig(GenerateShadowCameraConfig(gpm, shadowMapEntity, shadowMapCmpt));

		fr::CameraComponent::MarkDirty(gpm, cameraEntity);
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