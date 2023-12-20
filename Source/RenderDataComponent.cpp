// © 2023 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "RenderDataComponent.h"
#include "RenderManager.h"
#include "RenderSystem.h"


namespace gr
{
	std::atomic<gr::RenderObjectID> RenderDataComponent::s_objectIDs = 0;


	RenderDataComponent& RenderDataComponent::AttachNewRenderDataComponent(
		fr::GameplayManager& gpm, entt::entity entity, TransformID transformID)
	{
		gpm.EmplaceComponent<gr::RenderDataComponent::NewIDMarker>(entity);
		return *gpm.EmplaceComponent<gr::RenderDataComponent>(entity, PrivateCTORTag{}, transformID);
	}


	RenderDataComponent& RenderDataComponent::AttachSharedRenderDataComponent(
		fr::GameplayManager& gpm, entt::entity entity, RenderDataComponent const& renderDataComponent)
	{
		return *gpm.EmplaceComponent<gr::RenderDataComponent>(
			entity, 
			PrivateCTORTag{},
			renderDataComponent.m_renderObjectID, 
			renderDataComponent.m_transformID);
	}


	// ---


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, gr::TransformID transformID)
		: m_renderObjectID(s_objectIDs.fetch_add(1)) // Allocate a new RenderObjectID
		, m_transformID(transformID)
	{
	}


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, gr::RenderObjectID renderObjectID, gr::TransformID transformID)
		: m_renderObjectID(renderObjectID)
		, m_transformID(transformID)
	{
	}


	RenderDataComponent::RenderDataComponent(PrivateCTORTag, RenderDataComponent const& sharedRenderDataComponent)
		: m_renderObjectID(sharedRenderDataComponent.m_renderObjectID) // Shared RenderObjectID
		, m_transformID(sharedRenderDataComponent.m_transformID)
	{
	}


	gr::RenderObjectID RenderDataComponent::GetRenderObjectID() const
	{
		return m_renderObjectID;
	}



	gr::TransformID RenderDataComponent::GetTransformID() const
	{
		return m_transformID;
	}


	// ---


	RegisterRenderObjectCommand::RegisterRenderObjectCommand(RenderDataComponent const& newRenderDataComponent)
		: m_objectID(newRenderDataComponent.GetRenderObjectID())
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
			gr::RenderData& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.RegisterObject(cmdPtr->m_objectID, cmdPtr->m_transformID);
		}
	}


	void RegisterRenderObjectCommand::Destroy(void* cmdData)
	{
		RegisterRenderObjectCommand* cmdPtr = reinterpret_cast<RegisterRenderObjectCommand*>(cmdData);
		cmdPtr->~RegisterRenderObjectCommand();
	}


	// ---


	DestroyRenderObjectCommand::DestroyRenderObjectCommand(gr::RenderObjectID objectID)
		: m_objectID(objectID)
	{
	}


	void DestroyRenderObjectCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		DestroyRenderObjectCommand* cmdPtr = reinterpret_cast<DestroyRenderObjectCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderData& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.DestroyObject(cmdPtr->m_objectID);
		}
	}


	void DestroyRenderObjectCommand::Destroy(void* cmdData)
	{
		DestroyRenderObjectCommand* cmdPtr = reinterpret_cast<DestroyRenderObjectCommand*>(cmdData);
		cmdPtr->~DestroyRenderObjectCommand();
	}
}