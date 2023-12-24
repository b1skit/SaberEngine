// © 2023 Adam Badke. All rights reserved.

#include "CameraComponent.h"
#include "GameplayManager.h"
#include "MarkerComponents.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"


namespace fr
{
	CameraComponent& CameraComponent::AttachCameraComponent(
		fr::GameplayManager& gpm, entt::entity owningEntity, char const* name, gr::Camera::Config const& cameraConfig)
	{
		SEAssert("A camera's owning entity requires a TransformComponent",
			gpm.IsInHierarchyAbove<fr::TransformComponent>(owningEntity));

		entt::entity cameraEntity = gpm.CreateEntity(name);

		// Relationship:
		fr::Relationship& cameraRelationship = gpm.GetComponent<fr::Relationship>(cameraEntity);
		cameraRelationship.SetParent(gpm, owningEntity);

		// Find the Transform in the hierarchy above us
		fr::TransformComponent* transformCmpt =
			gpm.GetFirstInHierarchyAbove<fr::TransformComponent>(cameraRelationship.GetParent());
		const gr::TransformID transformID = transformCmpt->GetTransformID();

		// Get an attached RenderDataComponent, or create one if none exists:
		gr::RenderDataComponent* cameraRenderDataCmpt = 
			gpm.GetFirstInHierarchyAbove<gr::RenderDataComponent>(cameraRelationship.GetParent());
		if (cameraRenderDataCmpt == nullptr)
		{
			cameraRenderDataCmpt = 
				&gr::RenderDataComponent::AttachNewRenderDataComponent(gpm, cameraEntity, transformID);
		}
		else
		{
			gr::RenderDataComponent::AttachSharedRenderDataComponent(gpm, cameraEntity, *cameraRenderDataCmpt);
		}
		
		// Camera component:
		gpm.EmplaceComponent<fr::CameraComponent>(cameraEntity, PrivateCTORTag{}, cameraConfig, *transformCmpt);
		fr::CameraComponent& cameraComponent = gpm.GetComponent<fr::CameraComponent>(cameraEntity);

		// Mark our new MeshPrimitive as dirty:
		gpm.EmplaceComponent<DirtyMarker<fr::CameraComponent>>(cameraEntity);

		// Note: A Material component must be attached to the returned entity
		return cameraComponent;
	}


	CameraComponent::CameraComponent(
		PrivateCTORTag, gr::Camera::Config const& cameraConfig, fr::TransformComponent& transformCmpt)
		: m_camera(
			"EXPLICIT NAME IS DEPRECATED",
			cameraConfig,
			&transformCmpt.GetTransform(),
			true) // isComponent IS DEPRECATED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		, m_transformID(transformCmpt.GetTransformID())
	{
	}


	gr::Camera::RenderData CameraComponent::CreateRenderData(CameraComponent& cameraComponent)
	{
		return gr::Camera::RenderData{
			.m_cameraConfig = cameraComponent.GetCamera().GetCameraConfig(),
			.m_cameraParams = fr::CameraComponent::BuildCameraParams(cameraComponent.GetCamera()),
			.m_transformID = cameraComponent.GetTransformID()
		};
	}


	gr::Camera::CameraParams CameraComponent::BuildCameraParams(fr::Camera& camera)
	{
		gr::Camera::Config const& cameraConfig = camera.GetCameraConfig();
		fr::Transform& transform = *camera.GetTransform();

		gr::Camera::CameraParams cameraParams{};

		glm::mat4 const& globalMatrix = transform.GetGlobalMatrix();
		cameraParams.g_view = glm::inverse(globalMatrix);
		cameraParams.g_invView = globalMatrix;

		switch (cameraConfig.m_projectionType)
		{
		case gr::Camera::Config::ProjectionType::Perspective:
		case gr::Camera::Config::ProjectionType::PerspectiveCubemap:
		{
			cameraParams.g_projection = gr::Camera::BuildPerspectiveProjectionMatrix(
				cameraConfig.m_yFOV,
				cameraConfig.m_aspectRatio,
				cameraConfig.m_near,
				cameraConfig.m_far);

			cameraParams.g_invProjection = glm::inverse(cameraParams.g_projection);
		}
		break;
		case gr::Camera::Config::ProjectionType::Orthographic:
		{
			cameraParams.g_projection = gr::Camera::BuildOrthographicProjectionMatrix(
				cameraConfig.m_orthoLeftRightBotTop.x,
				cameraConfig.m_orthoLeftRightBotTop.y,
				cameraConfig.m_orthoLeftRightBotTop.z,
				cameraConfig.m_orthoLeftRightBotTop.w,
				cameraConfig.m_near,
				cameraConfig.m_far);

			cameraParams.g_invProjection = glm::inverse(cameraParams.g_projection);
		}
		break;
		default: SEAssertF("Invalid projection type");
		}

		cameraParams.g_viewProjection = cameraParams.g_projection * cameraParams.g_view;
		cameraParams.g_invViewProjection = glm::inverse(cameraParams.g_viewProjection);

		// .x = near, .y = far, .z = 1/near, .w = 1/far
		cameraParams.g_projectionParams = glm::vec4(
			cameraConfig.m_near,
			cameraConfig.m_far,
			1.f / cameraConfig.m_near,
			1.f / cameraConfig.m_far);

		const float ev100 = gr::Camera::ComputeEV100FromExposureSettings(
			cameraConfig.m_aperture,
			cameraConfig.m_shutterSpeed,
			cameraConfig.m_sensitivity,
			cameraConfig.m_exposureCompensation);

		cameraParams.g_exposureProperties = glm::vec4(
			gr::Camera::ComputeExposure(ev100),
			ev100,
			0.f,
			0.f);

		const float bloomEV100 = gr::Camera::ComputeEV100FromExposureSettings(
			cameraConfig.m_aperture,
			cameraConfig.m_shutterSpeed,
			cameraConfig.m_sensitivity,
			cameraConfig.m_bloomExposureCompensation);

		cameraParams.g_bloomSettings = glm::vec4(
			cameraConfig.m_bloomStrength,
			cameraConfig.m_bloomRadius.x,
			cameraConfig.m_bloomRadius.y,
			gr::Camera::ComputeExposure(bloomEV100));

		cameraParams.g_cameraWPos = glm::vec4(transform.GetGlobalPosition().xyz, 0.f);

		return cameraParams;
	}


	void CameraComponent::MarkDirty(GameplayManager& gpm, entt::entity cameraEntity)
	{
		gpm.TryEmplaceComponent<DirtyMarker<fr::CameraComponent>>(cameraEntity);
		
		// Note: We don't explicitely set the fr::Camera dirty flag. Having a DirtyMarker is all that's required to 
		// force an update
	}
}