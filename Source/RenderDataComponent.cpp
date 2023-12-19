// © 2023 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "RenderDataComponent.h"
#include "RenderManager.h"
#include "RenderSystem.h"


namespace gr
{
	std::atomic<gr::RenderObjectID> RenderDataComponent::s_objectIDs = 0;


	RenderDataComponent& RenderDataComponent::AttachRenderDataComponent(
		fr::GameplayManager& gpm, entt::entity entity, uint32_t expectedNumPrimitives)
	{
		RenderDataComponent* renderDataComponent = gpm.TryGetComponent<gr::RenderDataComponent>(entity);
		if (renderDataComponent == nullptr)
		{
			renderDataComponent =
				gpm.EmplaceComponent<gr::RenderDataComponent>(entity, RenderDataComponent(expectedNumPrimitives));
		}
		else
		{
			// RenderDataComponent already exists: Increase the reservation size
			for (uint32_t primIdx = 0; primIdx < expectedNumPrimitives; primIdx++)
			{
				renderDataComponent->AddRenderObject();
			}
		}		

		return *renderDataComponent;
	}


	RenderDataComponent::RenderDataComponent(uint32_t expectedNumPrimitives)
	{
		{
			std::unique_lock<std::shared_mutex> lock(m_objectIDsMutex);

			m_objectIDs.reserve(expectedNumPrimitives);
			for (uint32_t primitive = 0; primitive < expectedNumPrimitives; primitive++)
			{
				m_objectIDs.emplace_back(s_objectIDs.fetch_add(1));
			}
		}
	}


	RenderDataComponent::RenderDataComponent(RenderDataComponent&& rhs) noexcept
	{
		*this = std::move(rhs);
	}


	RenderDataComponent& RenderDataComponent::operator=(RenderDataComponent&& rhs) noexcept
	{
		{
			std::scoped_lock lock(m_objectIDsMutex, rhs.m_objectIDsMutex);

			m_objectIDs = std::move(rhs.m_objectIDs);

			return *this;
		}
	}


	size_t RenderDataComponent::GetNumRenderObjectIDs() const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_objectIDsMutex);
			return m_objectIDs.size();
		}
	}


	gr::RenderObjectID RenderDataComponent::GetRenderObjectID(size_t index) const
	{
		{
			std::shared_lock<std::shared_mutex> readLock(m_objectIDsMutex);
			return m_objectIDs.at(index);
		}
	}


	void RenderDataComponent::AddRenderObject()
	{
		{
			std::unique_lock<std::shared_mutex> writeLock(m_objectIDsMutex);
			m_objectIDs.emplace_back(s_objectIDs.fetch_add(1));
		}
	}


	// ---


	CreateRenderObjectCommand::CreateRenderObjectCommand(gr::RenderObjectID objectID)
		: m_objectID(objectID)
	{
	}


	void CreateRenderObjectCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		CreateRenderObjectCommand* cmdPtr = reinterpret_cast<CreateRenderObjectCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderData& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.RegisterObject(cmdPtr->m_objectID);
		}
	}


	void CreateRenderObjectCommand::Destroy(void* cmdData)
	{
		CreateRenderObjectCommand* cmdPtr = reinterpret_cast<CreateRenderObjectCommand*>(cmdData);
		cmdPtr->~CreateRenderObjectCommand();
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