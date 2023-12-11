// © 2023 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "GameplayManager.h"
#include "MarkerComponents.h"
#include "RenderManager.h"
#include "RenderSystem.h"


namespace fr
{
	void CreateSceneBoundsEntity(fr::GameplayManager& gpm)
	{
		entt::entity sceneBoundsEntity = gpm.CreateEntity();
		gpm.EmplaceComponent<gr::Bounds>(sceneBoundsEntity);
		gpm.EmplaceComponent<DirtyMarker<gr::Bounds>>(sceneBoundsEntity);
		gpm.EmplaceComponent<gr::RenderDataComponent>(sceneBoundsEntity);
		gpm.EmplaceComponent<IsSceneBoundsMarker>(sceneBoundsEntity);
	}


	void AttachBoundsComponent(fr::GameplayManager& gpm, entt::entity entity)
	{
		gpm.TryEmplaceComponent<gr::Bounds>(entity);
		gpm.EmplaceOrReplaceComponent<DirtyMarker<gr::Bounds>>(entity);
	}


	// ---


	UpdateBoundsDataRenderCommand::UpdateBoundsDataRenderCommand(
		gr::RenderObjectID objectID, gr::Bounds const& bounds)
		: m_objectID(objectID)
		, m_boundsData(bounds)
	{
	}


	void UpdateBoundsDataRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		UpdateBoundsDataRenderCommand* cmdPtr = reinterpret_cast<UpdateBoundsDataRenderCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderData& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.SetObjectData(cmdPtr->m_objectID, &cmdPtr->m_boundsData);
		}
	}


	void UpdateBoundsDataRenderCommand::Destroy(void* cmdData)
	{
		UpdateBoundsDataRenderCommand* cmdPtr = reinterpret_cast<UpdateBoundsDataRenderCommand*>(cmdData);
		cmdPtr->~UpdateBoundsDataRenderCommand();
	}


	// ---


	DestroyBoundsDataRenderCommand::DestroyBoundsDataRenderCommand(gr::RenderObjectID objectID)
		: m_objectID(objectID)
	{
	}


	void DestroyBoundsDataRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		DestroyBoundsDataRenderCommand* cmdPtr = reinterpret_cast<DestroyBoundsDataRenderCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderData& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.DestroyObjectData<gr::Bounds>(cmdPtr->m_objectID);
		}
	}


	void DestroyBoundsDataRenderCommand::Destroy(void* cmdData)
	{
		DestroyBoundsDataRenderCommand* cmdPtr = reinterpret_cast<DestroyBoundsDataRenderCommand*>(cmdData);
		cmdPtr->~DestroyBoundsDataRenderCommand();
	}
}