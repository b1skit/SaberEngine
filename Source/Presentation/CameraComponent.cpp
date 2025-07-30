// © 2023 Adam Badke. All rights reserved.
#include "CameraComponent.h"
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "NameComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"

#include "Renderer/RenderSystem.h"


namespace pr
{
	void CameraComponent::CreateCameraConcept(
		pr::EntityManager& em, entt::entity sceneNode, std::string_view name, gr::Camera::Config const& cameraConfig)
	{
		SEAssert(sceneNode != entt::null, "Cannot attach a CameraComponent to a null sceneNode");

		SEAssert(em.HasComponent<pr::TransformComponent>(sceneNode),
			"A CameraComponent must be attached to an entity that has a TransformComponent");

		pr::TransformComponent& owningTransform = em.GetComponent<pr::TransformComponent>(sceneNode);

		pr::RenderDataComponent::GetCreateRenderDataComponent(em, sceneNode, owningTransform.GetTransformID());

		// CameraComponent:
		pr::CameraComponent* cameraComponent = 
			em.EmplaceComponent<pr::CameraComponent>(sceneNode, PrivateCTORTag{}, cameraConfig, owningTransform);

		cameraComponent->MarkDirty(em, sceneNode);
	}


	CameraComponent& CameraComponent::AttachCameraComponent(
		pr::EntityManager& em, entt::entity owningEntity, std::string_view name, gr::Camera::Config const& cameraConfig)
	{
		SEAssert(owningEntity != entt::null, "Cannot attach a CameraComponent to a null entity");

		SEAssert(em.HasComponent<pr::TransformComponent>(owningEntity),
			"A CameraComponent must be attached to an entity that has a TransformComponent");

		SEAssert(em.HasComponent<pr::RenderDataComponent>(owningEntity),
			"A CameraComponent must be attached to an entity that has a RenderDataComponent");

		pr::TransformComponent& owningTransform = em.GetComponent<pr::TransformComponent>(owningEntity);

		// CameraComponent:
		pr::CameraComponent* cameraComponent = 
			em.EmplaceComponent<pr::CameraComponent>(owningEntity, PrivateCTORTag{}, cameraConfig, owningTransform);

		cameraComponent->MarkDirty(em, owningEntity);

		return *cameraComponent;
	}


	void CameraComponent::MarkDirty(EntityManager& em, entt::entity cameraEntity)
	{
		em.TryEmplaceComponent<DirtyMarker<pr::CameraComponent>>(cameraEntity);
	}


	gr::Camera::RenderData CameraComponent::CreateRenderData(
		pr::EntityManager& em, entt::entity entity, CameraComponent const& cameraComponent)
	{
		pr::NameComponent const& nameCmpt = em.GetComponent<pr::NameComponent>(entity);

		gr::Camera::RenderData renderData = gr::Camera::RenderData{
			.m_cameraParams = pr::Camera::BuildCameraData(cameraComponent.GetCamera()),
			.m_cameraConfig = cameraComponent.GetCamera().GetCameraConfig(),
			.m_transformID = cameraComponent.GetTransformID(),
			.m_isActive = cameraComponent.GetCamera().IsActive(),
		};

		strncpy(renderData.m_cameraName, nameCmpt.GetName().c_str(), core::INamedObject::k_maxNameLength);

		return renderData;
	}


	void CameraComponent::ShowImGuiWindow(pr::EntityManager& em, entt::entity camEntity)
	{
		pr::NameComponent const& nameCmpt = em.GetComponent<pr::NameComponent>(camEntity);

		if (ImGui::CollapsingHeader(
			std::format("Camera \"{}\"##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			pr::RenderDataComponent::ShowImGuiWindow(em, camEntity);
			
			pr::CameraComponent& camCmpt = em.GetComponent<pr::CameraComponent>(camEntity);
			camCmpt.m_camera.ShowImGuiWindow(nameCmpt.GetUniqueID());

			pr::TransformComponent& camTransformCmpt = em.GetComponent<pr::TransformComponent>(camEntity);
			pr::TransformComponent::ShowImGuiWindow(em, camEntity, static_cast<uint32_t>(camEntity));

			ImGui::Unindent();
		}
	}


	CameraComponent::CameraComponent(
		PrivateCTORTag, gr::Camera::Config const& cameraConfig, pr::TransformComponent& transformCmpt)
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
		SetActiveCameraRenderCommand* cmdPtr = reinterpret_cast<SetActiveCameraRenderCommand*>(cmdData);

		std::vector<std::unique_ptr<gr::RenderSystem>> const& renderSystems = cmdPtr->GetRenderSystems();

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::GraphicsSystemManager& gsm = renderSystems[renderSystemIdx]->GetGraphicsSystemManager();

			gsm.SetActiveCamera(cmdPtr->m_cameraRenderDataID, cmdPtr->m_cameraTransformID);
		}
	}
}