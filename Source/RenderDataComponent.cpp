// © 2023 Adam Badke. All rights reserved.
#include "RenderDataComponent.h"
#include "RenderManager.h"
#include "RenderSystem.h"

namespace gr
{
	std::atomic<gr::RenderObjectID> RenderDataComponent::s_objectIDs = 0;


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