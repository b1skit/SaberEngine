// © 2023 Adam Badke. All rights reserved.
#include "Private/CameraComponent.h"
#include "Private/EntityManager.h"
#include "Private/MarkerComponents.h"
#include "Private/NameComponent.h"
#include "Private/RenderDataComponent.h"
#include "Private/TransformComponent.h"


namespace fr
{
	void CameraComponent::CreateCameraConcept(
		fr::EntityManager& em, entt::entity sceneNode, char const* name, gr::Camera::Config const& cameraConfig)
	{
		SEAssert(sceneNode != entt::null, "Cannot attach a CameraComponent to a null sceneNode");

		SEAssert(em.HasComponent<fr::TransformComponent>(sceneNode),
			"A CameraComponent must be attached to an entity that has a TransformComponent");

		fr::TransformComponent& owningTransform = em.GetComponent<fr::TransformComponent>(sceneNode);

		fr::RenderDataComponent::GetCreateRenderDataComponent(em, sceneNode, owningTransform.GetTransformID());

		// CameraComponent:
		fr::CameraComponent* cameraComponent = 
			em.EmplaceComponent<fr::CameraComponent>(sceneNode, PrivateCTORTag{}, cameraConfig, owningTransform);

		cameraComponent->MarkDirty(em, sceneNode);
	}


	void CameraComponent::AttachCameraComponent(
		fr::EntityManager& em, entt::entity owningEntity, char const* name, gr::Camera::Config const& cameraConfig)
	{
		SEAssert(owningEntity != entt::null, "Cannot attach a CameraComponent to a null entity");

		SEAssert(em.HasComponent<fr::TransformComponent>(owningEntity),
			"A CameraComponent must be attached to an entity that has a TransformComponent");

		SEAssert(em.HasComponent<fr::RenderDataComponent>(owningEntity),
			"A CameraComponent must be attached to an entity that has a RenderDataComponent");

		fr::TransformComponent& owningTransform = em.GetComponent<fr::TransformComponent>(owningEntity);

		// CameraComponent:
		fr::CameraComponent* cameraComponent = 
			em.EmplaceComponent<fr::CameraComponent>(owningEntity, PrivateCTORTag{}, cameraConfig, owningTransform);

		cameraComponent->MarkDirty(em, owningEntity);
	}


	void CameraComponent::AttachCameraComponent(
		fr::EntityManager& em, entt::entity owningEntity, std::string const& name, gr::Camera::Config const& camConfig)
	{
		AttachCameraComponent(em, owningEntity, name.c_str(), camConfig);
	}


	void CameraComponent::MarkDirty(EntityManager& em, entt::entity cameraEntity)
	{
		em.TryEmplaceComponent<DirtyMarker<fr::CameraComponent>>(cameraEntity);
	}


	gr::Camera::RenderData CameraComponent::CreateRenderData(entt::entity entity, CameraComponent const& cameraComponent)
	{
		fr::EntityManager const* em = fr::EntityManager::Get();
		fr::NameComponent const& nameCmpt = em->GetComponent<fr::NameComponent>(entity);

		gr::Camera::RenderData renderData = gr::Camera::RenderData{
			.m_cameraConfig = cameraComponent.GetCamera().GetCameraConfig(),
			.m_cameraParams = fr::Camera::BuildCameraData(cameraComponent.GetCamera()),
			.m_transformID = cameraComponent.GetTransformID()
		};

		strncpy(renderData.m_cameraName, nameCmpt.GetName().c_str(), core::INamedObject::k_maxNameLength);

		return renderData;
	}


	void CameraComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity camEntity)
	{
		fr::NameComponent const& nameCmpt = em.GetComponent<fr::NameComponent>(camEntity);

		if (ImGui::CollapsingHeader(
			std::format("Camera \"{}\"##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			fr::RenderDataComponent::ShowImGuiWindow(em, camEntity);
			
			fr::CameraComponent& camCmpt = em.GetComponent<fr::CameraComponent>(camEntity);
			camCmpt.m_camera.ShowImGuiWindow(nameCmpt.GetUniqueID());

			fr::TransformComponent& camTransformCmpt = em.GetComponent<fr::TransformComponent>(camEntity);
			fr::TransformComponent::ShowImGuiWindow(em, camEntity, static_cast<uint32_t>(camEntity));

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
		std::vector<std::unique_ptr<gr::RenderSystem>> const& renderSystems =
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