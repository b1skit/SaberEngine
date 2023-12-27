// © 2023 Adam Badke. All rights reserved.
#include "CameraComponent.h"
#include "GameplayManager.h"
#include "MarkerComponents.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"


namespace fr
{
	entt::entity CameraComponent::AttachCameraConcept(
		fr::GameplayManager& gpm, entt::entity owningEntity, char const* name, gr::Camera::Config const& cameraConfig)
	{
		SEAssert("A camera's owning entity requires a TransformComponent",
			gpm.IsInHierarchyAbove<fr::TransformComponent>(owningEntity));

		entt::entity cameraEntity = gpm.CreateEntity(name);

		// Relationship:
		fr::Relationship& cameraRelationship = gpm.GetComponent<fr::Relationship>(cameraEntity);
		cameraRelationship.SetParent(gpm, owningEntity);

		// Find a Transform in the hierarchy above us
		fr::TransformComponent* transformCmpt =
			gpm.GetFirstInHierarchyAbove<fr::TransformComponent>(cameraRelationship.GetParent());

		// Get an attached RenderDataComponent, or create one if none exists:
		gr::RenderDataComponent* cameraRenderDataCmpt = 
			gpm.GetFirstInHierarchyAbove<gr::RenderDataComponent>(cameraRelationship.GetParent());
		if (cameraRenderDataCmpt == nullptr)
		{
			const gr::TransformID transformID = transformCmpt->GetTransformID();

			cameraRenderDataCmpt = 
				&gr::RenderDataComponent::AttachNewRenderDataComponent(gpm, cameraEntity, transformID);
		}
		else
		{
			gr::RenderDataComponent::AttachSharedRenderDataComponent(gpm, cameraEntity, *cameraRenderDataCmpt);
		}
		
		// Camera component:
		gpm.EmplaceComponent<fr::CameraComponent>(cameraEntity, PrivateCTORTag{}, cameraConfig, *transformCmpt);

		// Mark our new camera as dirty:
		gpm.EmplaceComponent<DirtyMarker<fr::CameraComponent>>(cameraEntity);

		return cameraEntity;
	}


	entt::entity CameraComponent::AttachCameraConcept(
		fr::GameplayManager& gpm, entt::entity owningEntity, std::string const& name, gr::Camera::Config const& camConfig)
	{
		return AttachCameraConcept(gpm, owningEntity, name.c_str(), camConfig);
	}


	void CameraComponent::MarkDirty(GameplayManager& gpm, entt::entity cameraEntity)
	{
		gpm.TryEmplaceComponent<DirtyMarker<fr::CameraComponent>>(cameraEntity);

		// Note: We don't explicitely set the fr::Camera dirty flag. Having a DirtyMarker is all that's required to 
		// force an update
	}


	gr::Camera::RenderData CameraComponent::CreateRenderData(CameraComponent const& cameraComponent)
	{
		return gr::Camera::RenderData{
			.m_cameraConfig = cameraComponent.GetCamera().GetCameraConfig(),
			.m_cameraParams = fr::Camera::BuildCameraParams(cameraComponent.GetCamera()),
			.m_transformID = cameraComponent.GetTransformID()
		};
	}


	CameraComponent::CameraComponent(
		PrivateCTORTag, gr::Camera::Config const& cameraConfig, fr::TransformComponent& transformCmpt)
		: m_transformID(transformCmpt.GetTransformID())
		, m_camera(cameraConfig, &transformCmpt.GetTransform())
	{
	}


	// ---


	SetActiveCameraRenderCommand::SetActiveCameraRenderCommand(
		gr::RenderDataID cameraRenderDataID, gr::TransformID cameraTransformID)
		: m_cameraRenderDataID(cameraRenderDataID)
		, m_cameraTransformID(cameraTransformID)
	{
	}


	void SetActiveCameraRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		SetActiveCameraRenderCommand* cmdPtr = reinterpret_cast<SetActiveCameraRenderCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::GraphicsSystemManager& gsm = renderSystems[renderSystemIdx]->GetGraphicsSystemManager();

			gsm.SetActiveCamera(cmdPtr->m_cameraRenderDataID, cmdPtr->m_cameraTransformID);
		}
	}


	void SetActiveCameraRenderCommand::Destroy(void* cmdData)
	{
		SetActiveCameraRenderCommand* cmdPtr = reinterpret_cast<SetActiveCameraRenderCommand*>(cmdData);
		cmdPtr->~SetActiveCameraRenderCommand();
	}
}