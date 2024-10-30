// © 2023 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "CameraComponent.h"
#include "Core/Config.h"
#include "EntityManager.h"
#include "LightComponent.h"
#include "MarkerComponents.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "ShadowMapComponent.h"
#include "TransformComponent.h"

#include "Renderer/RenderManager.h"


namespace
{
	constexpr float k_defaultShadowCamNear = 0.1f;


	gr::Camera::Config SnapTransformAndComputeDirectionalShadowCameraConfig(
		fr::ShadowMap const& shadowMap,
		fr::Transform& lightTransform,
		fr::BoundsComponent const* sceneWorldBounds,
		fr::CameraComponent const* activeSceneCam)
	{
		SEAssert(shadowMap.GetShadowMapType() == fr::ShadowMap::ShadowType::Orthographic, "Unexpected shadow map type");
		
		// TODO: Make the padding around orthographic shadow map edges tuneable
		constexpr float k_padding = 1.f;
		constexpr float k_defaultNearDist = 1.f;

		gr::Camera::Config shadowCamConfig{};
		shadowCamConfig.m_projectionType = gr::Camera::Config::ProjectionType::Orthographic;
		shadowCamConfig.m_yFOV = 0.f; // Not used

		fr::ShadowMap::TypeProperties directionalProperties =
			shadowMap.GetTypeProperties(fr::ShadowMap::ShadowType::Orthographic);

		switch (directionalProperties.m_orthographic.m_frustumSnapMode)
		{
		case fr::ShadowMap::TypeProperties::Orthographic::FrustumSnapMode::SceneBounds:
		{
			// Set the light's location so that it's oriented directly in the middle of the bounds, looking towards the
			// bounds region. This ensures the near and far planes are both on the same side of the X-axis, so that we
			// don't  have a view-space Z with a value of zero anywhere between near and far (and also just looks more
			// correct to have our light oriented towards its shadow camera frustum)
			if (sceneWorldBounds)
			{
				fr::BoundsComponent lightSpaceSceneBounds =
					sceneWorldBounds->GetTransformedAABBBounds(glm::inverse(lightTransform.GetGlobalMatrix()));

				glm::vec4 centerPoint = glm::vec4(
					(lightSpaceSceneBounds.xMin() + lightSpaceSceneBounds.xMax()) * 0.5f,
					(lightSpaceSceneBounds.yMin() + lightSpaceSceneBounds.yMax()) * 0.5f,
					lightSpaceSceneBounds.zMax() + k_defaultNearDist,
					1.f);

				centerPoint = lightTransform.GetGlobalMatrix() * centerPoint; // Light view -> world space

				lightTransform.SetGlobalPosition(centerPoint.xyz);

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
		case fr::ShadowMap::TypeProperties::Orthographic::FrustumSnapMode::ActiveCamera:
		{
			if (activeSceneCam && sceneWorldBounds)
			{
				fr::BoundsComponent const& lightSpaceSceneBounds =
					sceneWorldBounds->GetTransformedAABBBounds(glm::inverse(lightTransform.GetGlobalMatrix()));

				// Omit any scale components from the camera's view matrix
				fr::Transform const* sceneCamTransform = activeSceneCam->GetCamera().GetTransform();
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

namespace fr
{
	ShadowMapComponent& ShadowMapComponent::AttachShadowMapComponent(
		EntityManager& em, entt::entity owningEntity, char const* name, fr::Light::Type lightType)
	{
		SEAssert(em.HasComponent<fr::LightComponent>(owningEntity),
			"A ShadowMapComponent must be attached to a LightComponent");
		SEAssert(em.HasComponent<fr::RenderDataComponent>(owningEntity),
			"A ShadowMapComponent must be attached to an entity with a RenderDataComponent");

		fr::LightComponent const& owningLightComponent = em.GetComponent<fr::LightComponent>(owningEntity);
		fr::RenderDataComponent const& sharedRenderDataCmpt = em.GetComponent<fr::RenderDataComponent>(owningEntity);

		ShadowMapComponent& shadowMapComponent = *em.EmplaceComponent<fr::ShadowMapComponent>(
			owningEntity,
			PrivateCTORTag{},
			lightType,
			sharedRenderDataCmpt.GetRenderDataID(),
			sharedRenderDataCmpt.GetTransformID());

		fr::TransformComponent* owningTransform = em.GetFirstInHierarchyAbove<fr::TransformComponent>(owningEntity);
		SEAssert(owningTransform != nullptr, "A shadow map requires a TransformComponent");

		// We need to recompute the Transform, as it's likely dirty during scene construction
		owningTransform->GetTransform().Recompute();

		// Attach a shadow map render camera:
		fr::CameraComponent::AttachCameraComponent(
			em,
			owningEntity,
			std::format("{}_ShadowCam", name).c_str(),
			SnapTransformAndGenerateShadowCameraConfig(
				shadowMapComponent.GetShadowMap(), 
				owningTransform->GetTransform(), 
				owningLightComponent.GetLight(),
				nullptr,
				nullptr));

		// Add a shadow marker:
		em.EmplaceComponent<HasShadowMarker>(owningEntity);

		// Finally, mark our new ShadowMapComponent as dirty:
		em.EmplaceComponent<DirtyMarker<fr::ShadowMapComponent>>(owningEntity);

		return shadowMapComponent;
	}


	gr::Camera::Config ShadowMapComponent::SnapTransformAndGenerateShadowCameraConfig(
		ShadowMap const& shadowMap, 
		fr::Transform& lightTransform, 
		fr::Light const& owningLight, 
		fr::BoundsComponent const* sceneWorldBounds,
		fr::CameraComponent const* activeSceneCam)
	{
		gr::Camera::Config shadowCamConfig{};

		switch (shadowMap.GetShadowMapType())
		{
		case fr::ShadowMap::ShadowType::Orthographic:
		{
			// Note: It's valid for sceneWorldBounds to be null if it has not been created yet
			shadowCamConfig = SnapTransformAndComputeDirectionalShadowCameraConfig(
				shadowMap, lightTransform, sceneWorldBounds, activeSceneCam);
		}
		break;
		case fr::ShadowMap::ShadowType::Perspective:
		{
			SEAssert(owningLight.GetType() == fr::Light::Type::Spot, "Unexpected light type");

			fr::Light::TypeProperties const& lightTypeProperties = 
				owningLight.GetLightTypeProperties(fr::Light::Type::Spot);

			shadowCamConfig.m_projectionType = gr::Camera::Config::ProjectionType::Perspective;

			shadowCamConfig.m_yFOV = lightTypeProperties.m_spot.m_outerConeAngle * 2.f; // *2 for full light width
			
			shadowCamConfig.m_near = k_defaultShadowCamNear;
			shadowCamConfig.m_far = lightTypeProperties.m_spot.m_coneHeight;
			shadowCamConfig.m_aspectRatio = 1.f;
		}
		break;
		case fr::ShadowMap::ShadowType::CubeMap:
		{
			SEAssert(owningLight.GetType() == fr::Light::Type::Point, "Unexpected light type");

			shadowCamConfig.m_projectionType = gr::Camera::Config::ProjectionType::PerspectiveCubemap;

			shadowCamConfig.m_yFOV = static_cast<float>(std::numbers::pi) * 0.5f;		

			shadowCamConfig.m_near = k_defaultShadowCamNear;
			shadowCamConfig.m_far = owningLight.GetLightTypeProperties(fr::Light::Type::Point).m_point.m_sphericalRadius;
			shadowCamConfig.m_aspectRatio = 1.f;
		}
		break;
		default: SEAssertF("Invalid ShadowType");
		}

		return shadowCamConfig;
	}


	gr::ShadowMap::RenderData ShadowMapComponent::CreateRenderData(
		entt::entity entity, fr::ShadowMapComponent const& shadowMapCmpt)
	{
		fr::EntityManager const* em = fr::EntityManager::Get();
		fr::NameComponent const& nameCmpt = em->GetComponent<fr::NameComponent>(entity);

		fr::ShadowMap const& shadowMap = shadowMapCmpt.GetShadowMap();

		gr::ShadowMap::RenderData shadowRenderData = gr::ShadowMap::RenderData
		{
			.m_renderDataID = shadowMapCmpt.GetRenderDataID(),
			.m_transformID = shadowMapCmpt.GetTransformID(),

			.m_lightType = fr::Light::ConvertToGrLightType(shadowMap.GetOwningLightType()),
			.m_shadowType = fr::ShadowMap::GetGrShadowMapType(shadowMap.GetShadowMapType()),
			.m_shadowQuality = fr::ShadowMap::GetGrShadowQuality(shadowMap.GetShadowQuality()),

			.m_minMaxShadowBias = shadowMap.GetMinMaxShadowBias(),
			.m_softness = shadowMap.GetSoftness(),

			.m_shadowEnabled = shadowMap.IsEnabled(),
		};

		strncpy(shadowRenderData.m_owningLightName, nameCmpt.GetName().c_str(), core::INamedObject::k_maxNameLength);

		return shadowRenderData;
	}


	void ShadowMapComponent::Update(
		entt::entity entity,
		fr::ShadowMapComponent& shadowMapCmpt,
		fr::TransformComponent& lightTransformCmpt,
		fr::LightComponent const& lightCmpt,
		fr::CameraComponent& shadowCamCmpt, 
		fr::BoundsComponent const* sceneWorldBounds,
		fr::CameraComponent const* activeSceneCam,
		bool force)
	{
		bool didModify = false;

		fr::ShadowMap const& shadowMap = shadowMapCmpt.GetShadowMap();
		fr::ShadowMap::TypeProperties const& typeProperties = shadowMap.GetTypeProperties(shadowMap.GetShadowMapType());

		const bool mustUpdateFrustumSnap = (shadowMap.GetShadowMapType() == fr::ShadowMap::ShadowType::Orthographic &&
				typeProperties.m_orthographic.m_frustumSnapMode == 
			ShadowMap::TypeProperties::Orthographic::FrustumSnapMode::ActiveCamera) &&
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

			shadowMapCmpt.GetShadowMap().MarkClean();

			didModify = true;
		}

		if (didModify)
		{
			fr::EntityManager::Get()->TryEmplaceComponent<DirtyMarker<fr::ShadowMapComponent>>(entity);
		}
	}


	void ShadowMapComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity shadowMapEntity)
	{
		fr::NameComponent const& nameCmpt = em.GetComponent<fr::NameComponent>(shadowMapEntity);
		if (ImGui::CollapsingHeader(
			std::format("ShadowMap \"{}\"##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			fr::RenderDataComponent::ShowImGuiWindow(em, shadowMapEntity);

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
		gr::TransformID transformID)
		: m_renderDataID(renderDataID)
		, m_transformID(transformID)
		, m_shadowMap(lightType)
	{
	}
}