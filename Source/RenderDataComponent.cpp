// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "RenderDataComponent.h"
#include "RenderManager.h"
#include "RenderSystem.h"


namespace gr
{
	std::atomic<gr::RenderDataID> RenderDataComponent::s_objectIDs = 0;


	RenderDataComponent& RenderDataComponent::AttachNewRenderDataComponent(
		fr::EntityManager& em, entt::entity entity, TransformID transformID)
	{
		em.EmplaceComponent<gr::RenderDataComponent::NewRegistrationMarker>(entity);
		return *em.EmplaceComponent<gr::RenderDataComponent>(entity, PrivateCTORTag{}, transformID);
	}


	RenderDataComponent& RenderDataComponent::AttachSharedRenderDataComponent(
		fr::EntityManager& em, entt::entity entity, RenderDataComponent const& renderDataComponent)
	{
		em.EmplaceComponent<gr::RenderDataComponent::NewRegistrationMarker>(entity);
		return *em.EmplaceComponent<gr::RenderDataComponent>(
			entity, 
			PrivateCTORTag{},
			renderDataComponent.m_renderDataID, 
			renderDataComponent.m_transformID);
	}


	void RenderDataComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity)
	{
		ImGui::Indent();

		gr::RenderDataComponent const& renderDataCmpt = em.GetComponent<gr::RenderDataComponent>(owningEntity);
		ImGui::Text(std::format("RenderDataID: {}, TransformID: {}", 
			renderDataCmpt.GetRenderDataID(), renderDataCmpt.GetTransformID()).c_str());
		
		ImGui::Unindent();
	}


	// ---


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, gr::TransformID transformID)
		: m_renderDataID(s_objectIDs.fetch_add(1)) // Allocate a new RenderDataID
		, m_transformID(transformID)
	{
	}


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, gr::RenderDataID renderObjectID, gr::TransformID transformID)
		: m_renderDataID(renderObjectID)
		, m_transformID(transformID)
	{
	}


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, RenderDataComponent const& sharedRenderDataComponent)
		: m_renderDataID(sharedRenderDataComponent.m_renderDataID) // Shared RenderDataID
		, m_transformID(sharedRenderDataComponent.m_transformID)
	{
	}


	gr::RenderDataID RenderDataComponent::GetRenderDataID() const
	{
		return m_renderDataID;
	}



	gr::TransformID RenderDataComponent::GetTransformID() const
	{
		return m_transformID;
	}


	// ---


	RegisterRenderObjectCommand::RegisterRenderObjectCommand(RenderDataComponent const& newRenderDataComponent)
		: m_renderDataID(newRenderDataComponent.GetRenderDataID())
		, m_transformID(newRenderDataComponent.GetTransformID())
	{
	}


	void RegisterRenderObjectCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		RegisterRenderObjectCommand* cmdPtr = reinterpret_cast<RegisterRenderObjectCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderDataManager& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.RegisterObject(cmdPtr->m_renderDataID, cmdPtr->m_transformID);
		}
	}


	void RegisterRenderObjectCommand::Destroy(void* cmdData)
	{
		RegisterRenderObjectCommand* cmdPtr = reinterpret_cast<RegisterRenderObjectCommand*>(cmdData);
		cmdPtr->~RegisterRenderObjectCommand();
	}


	// ---


	DestroyRenderObjectCommand::DestroyRenderObjectCommand(gr::RenderDataID objectID)
		: m_renderDataID(objectID)
	{
	}


	void DestroyRenderObjectCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		DestroyRenderObjectCommand* cmdPtr = reinterpret_cast<DestroyRenderObjectCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderDataManager& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.DestroyObject(cmdPtr->m_renderDataID);
		}
	}


	void DestroyRenderObjectCommand::Destroy(void* cmdData)
	{
		DestroyRenderObjectCommand* cmdPtr = reinterpret_cast<DestroyRenderObjectCommand*>(cmdData);
		cmdPtr->~DestroyRenderObjectCommand();
	}
}