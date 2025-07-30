// © 2023 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "CameraComponent.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "MarkerComponents.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "ShadowMapComponent.h"
#include "TransformComponent.h"


namespace
{
	constexpr float k_defaultShadowCamNear = 0.1f;


	gr::Camera::Config SnapTransformAndComputeDirectionalShadowCameraConfig(
		pr::ShadowMap const& shadowMap,
		pr::Transform& lightTransform,
		pr::BoundsComponent const* sceneWorldBounds,
		pr::CameraComponent const* activeSceneCam)
	{
		SEAssert(shadowMap.GetShadowMapType() == pr::ShadowMap::ShadowType::Orthographic, "Unexpected shadow map type");
		
		// TODO: Make the padding around orthographic shadow map edges tuneable
		constexpr float k_padding = 1.f;
		constexpr float k_defaultNearDist = 1.f;

		gr::Camera::Config shadowCamConfig{};
		shadowCamConfig.m_projectionType = gr::Camera::Config::ProjectionType::Orthographic;
		shadowCamConfig.m_yFOV = 0.f; // Not used

		pr::ShadowMap::ShadowParams directionalProperties =
			shadowMap.GetTypeProperties(pr::ShadowMap::ShadowType::Orthographic);

		switch (directionalProperties.m_orthographic.m_frustumSnapMode)
		{
		case pr::ShadowMap::ShadowParams::Orthographic::FrustumSnapMode::SceneBounds:
		{
			// Set the light's location so that it's oriented directly in the middle of the bounds, looking towards the
			// bounds region. This ensures the near and far planes are both on the same side of the X-axis, so that we
			// don't  have a view-space Z with a value of zero anywhere between near and far (and also just looks more
			// correct to have our light oriented towards its shadow camera frustum)
			if (sceneWorldBounds)
			{
				pr::BoundsComponent lightSpaceSceneBounds =
					sceneWorldBounds->GetTransformedAABBBounds(glm::inverse(lightTransform.GetGlobalMatrix()));

				glm::vec4 centerPoint = glm::vec4(
					(lightSpaceSceneBounds.xMin() + lightSpaceSceneBounds.xMax()) * 0.5f,
					(lightSpaceSceneBounds.yMin() + lightSpaceSceneBounds.yMax()) * 0.5f,
					lightSpaceSceneBounds.zMax() + k_defaultNearDist,
					1.f);

				centerPoint = lightTransform.GetGlobalMatrix() * centerPoint; // Light view -> world space

				lightTransform.SetGlobalTranslation(centerPoint.xyz);

				lightSpaceSceneBounds = 
					sceneWorldBounds->GetTransformedAABBBounds(glm::inverse(lightTransform.GetGlobalMatrix()));

				/* As per the GLTF KHR_lights_punctual specs, directional lights emit light in the direction of the
				* local -Z axis:  
				* https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#directional
				* 
				* For an orthographic projection, the near (N) and far (F) planes can be at any point along the Z axis. 
				* Importantly, in our RHCS as we look in the direction of -Z, note that F < N in all cases. 
				*		 -Z
				*		  ^
				*		  |
				*		o---o F
				*		|	|
				*		o---o N
				*		  |
				* -X ----------- +X
				*		  |
				*		o---o F
				*		|	|
				*		o---o N
				*		  |
				*		 +Z
				* Our bounds are computed such that the "minimum" and "maximum" Z terms are oriented in the opposite
				* way. Thus, we must swap the min/max Z terms of our bounds, AND negate them to get the correct near/far
				* values:
				*/
				shadowCamConfig.m_near = -lightSpaceSceneBounds.zMax();
				shadowCamConfig.m_far = -lightSpaceSceneBounds.zMin();

				shadowCamConfig.m_orthoLeftRightBotTop.x = lightSpaceSceneBounds.xMin() - k_padding;
				shadowCamConfig.m_orthoLeftRightBotTop.y = lightSpaceSceneBounds.xMax() + k_padding;
				shadowCamConfig.m_orthoLeftRightBotTop.z = lightSpaceSceneBounds.yMin() - k_padding;
				shadowCamConfig.m_orthoLeftRightBotTop.w = lightSpaceSceneBounds.yMax() + k_padding;
			}
		}
		break;
		case pr::ShadowMap::ShadowParams::Orthographic::FrustumSnapMode::ActiveCamera:
		{
			if (activeSceneCam && sceneWorldBounds)
			{
				pr::BoundsComponent const& lightSpaceSceneBounds =
					sceneWorldBounds->GetTransformedAABBBounds(glm::inverse(lightTransform.GetGlobalMatrix()));

				// Omit any scale components from the camera's view matrix
				pr::Transform const* sceneCamTransform = activeSceneCam->GetCamera().GetTransform();
				glm::mat4 const& view = 
					glm::inverse(sceneCamTransform->GetGlobalTranslationMat() * sceneCamTransform->GetGlobalRotationMat());

				gr::Camera::Config const& sceneCamConfig = activeSceneCam->GetCamera().GetCameraConfig();

				glm::mat4 proj;
				switch (sceneCamConfig.m_projectionType)
				{
				case gr::Camera::Config::ProjectionType::Perspective:
				case gr::Camera::Config::ProjectionType::PerspectiveCubemap:
				{
					proj = gr::Camera::BuildPerspectiveProjectionMatrix(
						sceneCamConfig.m_yFOV, 
						sceneCamConfig.m_aspectRatio, 
						sceneCamConfig.m_near, 
						sceneCamConfig.m_far);
				}
				break;
				case gr::Camera::Config::ProjectionType::Orthographic:
				{
					proj = gr::Camera::BuildOrthographicProjectionMatrix(
						sceneCamConfig.m_orthoLeftRightBotTop,
						sceneCamConfig.m_near,
						sceneCamConfig.m_far);
				}
				break;
				default: SEAssertF("Invalid projection type");
				}

				// NDC -> world -> light space:
				glm::mat4 const& projToLightSpace = 
					glm::inverse(lightTransform.GetGlobalMatrix()) * glm::inverse(proj * view);

				float xMin = std::numeric_limits<float>::max();
				float xMax = std::numeric_limits<float>::min();
				float yMin = std::numeric_limits<float>::max();
				float yMax = std::numeric_limits<float>::min();
				float zMin = std::numeric_limits<float>::max();
				
				// Construct a cube in NDC space:
				std::array<glm::vec4, 8> frustumPoints = {
					glm::vec4(-1.f, 1.f, 1.f, 1.f),		// farTL
					glm::vec4(-1.f, -1.f, 1.f, 1.f),	// farBL
					glm::vec4(1.f, 1.f, 1.f, 1.f),		// farTR
					glm::vec4(1.f, -1.f, 1.f, 1.f),		// farBR
					glm::vec4(-1.f, 1.f, 0.f, 1.f),		// nearTL
					glm::vec4(-1.f, -1.f, 0.f, 1.f),	// nearBL
					glm::vec4(1.f, 1.f, 0.f, 1.f),		// nearTR
					glm::vec4(1.f, -1.f, 0.f, 1.f)};	// nearBR

				// Transform our camera frustum into light space:
				for (glm::vec4& point : frustumPoints)
				{
					point = projToLightSpace * point;
					point /= point.w;

					xMin = std::min(xMin, point.x);
					xMax = std::max(xMax, point.x);
					yMin = std::min(yMin, point.y);
					yMax = std::max(yMax, point.y);
					zMin = std::min(zMin, point.z);
				}

				// Clamp the frustum dimensions by taking the max(mins)/min(maxs):
				xMin = std::max(xMin, lightSpaceSceneBounds.xMin());
				xMax = std::min(xMax, lightSpaceSceneBounds.xMax());
				yMin = std::max(yMin, lightSpaceSceneBounds.yMin());
				yMax = std::min(yMax, lightSpaceSceneBounds.yMax());

				zMin = std::max(zMin, lightSpaceSceneBounds.zMin());

				// We start the frustum at the scene bounds to ensure shadows are correctly cast into the visible area
				shadowCamConfig.m_near = -lightSpaceSceneBounds.zMax();
				shadowCamConfig.m_far = -zMin;

				shadowCamConfig.m_orthoLeftRightBotTop.x = xMin - k_padding;
				shadowCamConfig.m_orthoLeftRightBotTop.y = xMax + k_padding;
				shadowCamConfig.m_orthoLeftRightBotTop.z = yMin - k_padding;
				shadowCamConfig.m_orthoLeftRightBotTop.w = yMax + k_padding;
			}
		}
		break;
		default: SEAssertF("Invalid snap mode");
		}

		return shadowCamConfig;
	}
}

namespace pr
{
	ShadowMapComponent& ShadowMapComponent::AttachShadowMapComponent(
		EntityManager& em, entt::entity owningEntity, char const* name, pr::Light::Type lightType)
	{
		SEAssert(em.HasComponent<pr::LightComponent>(owningEntity),
			"A ShadowMapComponent must be attached to a LightComponent");
		SEAssert(em.HasComponent<pr::RenderDataComponent>(owningEntity),
			"A ShadowMapComponent must be attached to an entity with a RenderDataComponent");

		pr::LightComponent const& owningLightComponent = em.GetComponent<pr::LightComponent>(owningEntity);
		pr::RenderDataComponent const& sharedRenderDataCmpt = em.GetComponent<pr::RenderDataComponent>(owningEntity);

		ShadowMapComponent& shadowMapComponent = *em.EmplaceComponent<pr::ShadowMapComponent>(
			owningEntity,
			PrivateCTORTag{},
			lightType,
			sharedRenderDataCmpt.GetRenderDataID(),
			sharedRenderDataCmpt.GetTransformID());

		pr::Relationship const& relationship = em.GetComponent<pr::Relationship>(owningEntity);
		pr::TransformComponent* owningTransform = relationship.GetFirstInHierarchyAbove<pr::TransformComponent>();
		SEAssert(owningTransform != nullptr, "A shadow map requires a TransformComponent");

		// We need to recompute the Transform, as it's likely dirty during scene construction
		owningTransform->GetTransform().Recompute();

		// Attach a shadow map render camera:
		pr::CameraComponent& shadowCamCmpt = pr::CameraComponent::AttachCameraComponent(
			em,
			owningEntity,
			std::format("{}_ShadowCam", name).c_str(),
			SnapTransformAndGenerateShadowCameraConfig(
				shadowMapComponent.GetShadowMap(), 
				owningTransform->GetTransform(), 
				owningLightComponent.GetLight(),
				nullptr,
				nullptr));

		// Activate the camera:
		shadowCamCmpt.GetCameraForModification().SetActive(true);
		
		// Add a shadow marker:
		em.EmplaceComponent<HasShadowMarker>(owningEntity);

		// Finally, mark our new ShadowMapComponent as dirty:
		em.EmplaceComponent<DirtyMarker<pr::ShadowMapComponent>>(owningEntity);

		return shadowMapComponent;
	}


	gr::Camera::Config ShadowMapComponent::SnapTransformAndGenerateShadowCameraConfig(
		ShadowMap const& shadowMap, 
		pr::Transform& lightTransform, 
		pr::Light const& owningLight, 
		pr::BoundsComponent const* sceneWorldBounds,
		pr::CameraComponent const* activeSceneCam)
	{
		gr::Camera::Config shadowCamConfig{};

		switch (shadowMap.GetShadowMapType())
		{
		case pr::ShadowMap::ShadowType::Orthographic:
		{
			// Note: It's valid for sceneWorldBounds to be null if it has not been created yet
			shadowCamConfig = SnapTransformAndComputeDirectionalShadowCameraConfig(
				shadowMap, lightTransform, sceneWorldBounds, activeSceneCam);
		}
		break;
		case pr::ShadowMap::ShadowType::Perspective:
		{
			SEAssert(owningLight.GetType() == pr::Light::Type::Spot, "Unexpected light type");

			pr::Light::TypeProperties const& lightTypeProperties = 
				owningLight.GetLightTypeProperties(pr::Light::Type::Spot);

			shadowCamConfig.m_projectionType = gr::Camera::Config::ProjectionType::Perspective;

			shadowCamConfig.m_yFOV = lightTypeProperties.m_spot.m_outerConeAngle * 2.f; // *2 for full light width
			
			shadowCamConfig.m_near = k_defaultShadowCamNear;
			shadowCamConfig.m_far = lightTypeProperties.m_spot.m_coneHeight;
			shadowCamConfig.m_aspectRatio = 1.f;
		}
		break;
		case pr::ShadowMap::ShadowType::CubeMap:
		{
			SEAssert(owningLight.GetType() == pr::Light::Type::Point, "Unexpected light type");

			shadowCamConfig.m_projectionType = gr::Camera::Config::ProjectionType::PerspectiveCubemap;

			shadowCamConfig.m_yFOV = static_cast<float>(std::numbers::pi) * 0.5f;		

			shadowCamConfig.m_near = k_defaultShadowCamNear;
			shadowCamConfig.m_far = owningLight.GetLightTypeProperties(pr::Light::Type::Point).m_point.m_sphericalRadius;
			shadowCamConfig.m_aspectRatio = 1.f;
		}
		break;
		default: SEAssertF("Invalid ShadowType");
		}

		return shadowCamConfig;
	}


	gr::ShadowMap::RenderData ShadowMapComponent::CreateRenderData(
		pr::EntityManager& em, entt::entity entity, pr::ShadowMapComponent const& shadowMapCmpt)
	{
		pr::NameComponent const& nameCmpt = em.GetComponent<pr::NameComponent>(entity);

		pr::ShadowMap const& shadowMap = shadowMapCmpt.GetShadowMap();

		gr::ShadowMap::RenderData shadowRenderData = gr::ShadowMap::RenderData
		{
			.m_renderDataID = shadowMapCmpt.GetRenderDataID(),
			.m_transformID = shadowMapCmpt.GetTransformID(),

			.m_lightType = pr::Light::ConvertToGrLightType(shadowMap.GetOwningLightType()),
			.m_shadowType = pr::ShadowMap::GetGrShadowMapType(shadowMap.GetShadowMapType()),
			.m_shadowQuality = pr::ShadowMap::GetGrShadowQuality(shadowMap.GetShadowQuality()),

			.m_minMaxShadowBias = shadowMap.GetMinMaxShadowBias(),
			.m_softness = shadowMap.GetSoftness(),

			.m_shadowEnabled = shadowMap.IsEnabled(),
		};

		strncpy(shadowRenderData.m_owningLightName, nameCmpt.GetName().c_str(), core::INamedObject::k_maxNameLength);

		return shadowRenderData;
	}


	void ShadowMapComponent::Update(
		entt::entity entity,
		pr::ShadowMapComponent& shadowMapCmpt,
		pr::TransformComponent& lightTransformCmpt,
		pr::LightComponent const& lightCmpt,
		pr::CameraComponent& shadowCamCmpt, 
		pr::BoundsComponent const* sceneWorldBounds,
		pr::CameraComponent const* activeSceneCam,
		bool force)
	{
		bool didModify = false;

		pr::ShadowMap const& shadowMap = shadowMapCmpt.GetShadowMap();
		pr::ShadowMap::ShadowParams const& typeProperties = shadowMap.GetTypeProperties(shadowMap.GetShadowMapType());

		const bool mustUpdateFrustumSnap = (shadowMap.GetShadowMapType() == pr::ShadowMap::ShadowType::Orthographic &&
				typeProperties.m_orthographic.m_frustumSnapMode == 
			ShadowMap::ShadowParams::Orthographic::FrustumSnapMode::ActiveCamera) &&
			activeSceneCam->GetCamera().GetTransform()->HasChanged();

		const bool mustUpdate = force ||
			shadowMap.IsDirty() ||
			mustUpdateFrustumSnap;
		
		if (mustUpdate)
		{
			shadowCamCmpt.GetCameraForModification().SetCameraConfig(
				SnapTransformAndGenerateShadowCameraConfig(
					shadowMapCmpt.GetShadowMap(), 
					lightTransformCmpt.GetTransform(),
					lightCmpt.GetLight(),
					sceneWorldBounds,
					activeSceneCam));

			// Ensure the shadow camera is active if the shadow map is enabled:
			shadowCamCmpt.GetCameraForModification().SetActive(shadowMapCmpt.GetShadowMap().IsEnabled());

			shadowMapCmpt.GetShadowMap().MarkClean();

			didModify = true;
		}

		if (didModify)
		{
			pr::EntityManager::Get()->TryEmplaceComponent<DirtyMarker<pr::ShadowMapComponent>>(entity);
		}
	}


	void ShadowMapComponent::ShowImGuiWindow(pr::EntityManager& em, entt::entity shadowMapEntity)
	{
		pr::NameComponent const& nameCmpt = em.GetComponent<pr::NameComponent>(shadowMapEntity);
		if (ImGui::CollapsingHeader(
			std::format("ShadowMap \"{}\"##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			pr::RenderDataComponent::ShowImGuiWindow(em, shadowMapEntity);

			pr::ShadowMapComponent& shadowMapCmpt = em.GetComponent<pr::ShadowMapComponent>(shadowMapEntity);
			shadowMapCmpt.GetShadowMap().ShowImGuiWindow(nameCmpt.GetUniqueID());

			// Shadow camera:
			pr::CameraComponent& shadowCamCmpt = em.GetComponent<pr::CameraComponent>(shadowMapEntity);
			ImGui::PushID(static_cast<uint64_t>(shadowMapEntity));
			pr::CameraComponent::ShowImGuiWindow(em, shadowMapEntity);
			ImGui::PopID();

			ImGui::Unindent();
		}
	}


	// ---


	ShadowMapComponent::ShadowMapComponent(
		PrivateCTORTag,
		pr::Light::Type lightType, 
		gr::RenderDataID renderDataID, 
		gr::TransformID transformID)
		: m_renderDataID(renderDataID)
		, m_transformID(transformID)
		, m_shadowMap(lightType)
	{
	}
}