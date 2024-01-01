// © 2023 Adam Badke. All rights reserved.
#include "CameraComponent.h"
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"


namespace fr
{
	entt::entity CameraComponent::AttachCameraConcept(
		fr::EntityManager& em, entt::entity owningEntity, char const* name, gr::Camera::Config const& cameraConfig)
	{
		SEAssert("A camera's owning entity requires a TransformComponent",
			em.IsInHierarchyAbove<fr::TransformComponent>(owningEntity));

		entt::entity cameraEntity = em.CreateEntity(name);

		// Relationship:
		fr::Relationship& cameraRelationship = em.GetComponent<fr::Relationship>(cameraEntity);
		cameraRelationship.SetParent(em, owningEntity);

		// Find a Transform in the hierarchy above us
		fr::TransformComponent* transformCmpt =
			em.GetFirstInHierarchyAbove<fr::TransformComponent>(cameraRelationship.GetParent());

		// Get an attached RenderDataComponent, or create one if none exists:
		gr::RenderDataComponent* cameraRenderDataCmpt = 
			em.GetFirstInHierarchyAbove<gr::RenderDataComponent>(cameraRelationship.GetParent());
		if (cameraRenderDataCmpt == nullptr)
		{
			const gr::TransformID transformID = transformCmpt->GetTransformID();

			cameraRenderDataCmpt = 
				&gr::RenderDataComponent::AttachNewRenderDataComponent(em, cameraEntity, transformID);
		}
		else
		{
			gr::RenderDataComponent::AttachSharedRenderDataComponent(em, cameraEntity, *cameraRenderDataCmpt);
		}
		
		// Camera component:
		em.EmplaceComponent<fr::CameraComponent>(cameraEntity, PrivateCTORTag{}, cameraConfig, *transformCmpt);

		// Mark our new camera as dirty:
		em.EmplaceComponent<DirtyMarker<fr::CameraComponent>>(cameraEntity);

		return cameraEntity;
	}


	entt::entity CameraComponent::AttachCameraConcept(
		fr::EntityManager& em, entt::entity owningEntity, std::string const& name, gr::Camera::Config const& camConfig)
	{
		return AttachCameraConcept(em, owningEntity, name.c_str(), camConfig);
	}


	void CameraComponent::MarkDirty(EntityManager& em, entt::entity cameraEntity)
	{
		em.TryEmplaceComponent<DirtyMarker<fr::CameraComponent>>(cameraEntity);

		// Note: We don't explicitely set the fr::Camera dirty flag. Having a DirtyMarker is all that's required to 
		// force an update
	}


	gr::Camera::RenderData CameraComponent::CreateRenderData(
		CameraComponent const& cameraComponent, fr::NameComponent const& nameCmpt)
	{
		gr::Camera::RenderData renderData = gr::Camera::RenderData{
			.m_cameraConfig = cameraComponent.GetCamera().GetCameraConfig(),
			.m_cameraParams = fr::Camera::BuildCameraParams(cameraComponent.GetCamera()),
			.m_transformID = cameraComponent.GetTransformID()
		};

		strncpy(renderData.m_cameraName, nameCmpt.GetName().c_str(), en::NamedObject::k_maxNameLength);

		return renderData;
	}


	void CameraComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity camEntity)
	{
		fr::NameComponent const& nameCmpt = em.GetComponent<fr::NameComponent>(camEntity);

		if (ImGui::CollapsingHeader(
			std::format("{}##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			gr::RenderDataComponent::ShowImGuiWindow(em, camEntity);
			
			fr::CameraComponent& camCmpt = em.GetComponent<fr::CameraComponent>(camEntity);
			
			fr::Relationship const& cameraRelationship = em.GetComponent<fr::Relationship>(camEntity);

			entt::entity transformEntity = entt::null;
			fr::TransformComponent& camTransformCmpt =
				*em.GetFirstAndEntityInHierarchyAbove<fr::TransformComponent>(cameraRelationship.GetParent(), transformEntity);

			camCmpt.m_camera.ShowImGuiWindow(nameCmpt.GetUniqueID());

			// Transform:
			fr::TransformComponent::ShowImGuiWindow(em, transformEntity, static_cast<uint32_t>(camEntity));

			ImGui::Unindent();
		}
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