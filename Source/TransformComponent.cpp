// © 2023 Adam Badke. All rights reserved.
#include "CoreEngine.h"
#include "GameplayManager.h"
#include "RenderData.h"
#include "RenderManager.h"
#include "TransformComponent.h"


namespace fr
{
	std::atomic<gr::TransformID> TransformComponent::s_transformIDs = 0;


	TransformComponent::RenderData TransformComponent::GetRenderData(gr::Transform& transform)
	{
		return RenderData{
			.g_model = transform.GetGlobalMatrix(),
			.g_transposeInvModel = glm::transpose(glm::inverse(transform.GetGlobalMatrix()))
		};
	}


	TransformComponent& TransformComponent::AttachTransformComponent(
		fr::GameplayManager& gpm, entt::entity entity, gr::Transform* parent)
	{
		gpm.EmplaceComponent<fr::TransformComponent::NewIDMarker>(entity);
		return *gpm.EmplaceComponent<fr::TransformComponent>(entity, PrivateCTORTag{}, parent);
	}


	TransformComponent::TransformComponent(PrivateCTORTag, gr::Transform* parent)
		: m_transformID(s_transformIDs.fetch_add(1))
		, m_transform(parent)
	{
	}


	// ---


	void TransformComponent::DispatchTransformUpdateThread(
		std::vector<std::future<void>>& taskFuturesOut, gr::Transform* rootNode)
	{
		// DFS walk down our Transform hierarchy, recomputing each Transform in turn. The goal here is to minimize the
		// (re)computation required when we copy Transforms for the Render thread

		taskFuturesOut.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
			[rootNode]()
			{
				fr::GameplayManager& gpm = *fr::GameplayManager::Get();

				std::stack<gr::Transform*> transforms;
				transforms.push(rootNode);

				while (!transforms.empty())
				{
					gr::Transform* topTransform = transforms.top();
					transforms.pop();

					topTransform->ClearHasChangedFlag();
					topTransform->Recompute();

					if (topTransform->HasChanged())
					{
						LOG("TFORM CHANGED"); // TEMP HAX!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


					}

					for (gr::Transform* child : topTransform->GetChildren())
					{
						transforms.push(child);
					}
				}
			}));
	}


	// ---


	UpdateTransformDataRenderCommand::UpdateTransformDataRenderCommand(fr::TransformComponent& transformComponent)
		: m_transformID(transformComponent.GetTransformID())
		, m_data(fr::TransformComponent::GetRenderData(transformComponent.GetTransform()))
	{
	}


	void UpdateTransformDataRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		UpdateTransformDataRenderCommand* cmdPtr = reinterpret_cast<UpdateTransformDataRenderCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderData& renderData =
				renderSystems[renderSystemIdx]->GetGraphicsSystemManager().GetRenderDataForModification();

			renderData.SetTransformData(cmdPtr->m_transformID, cmdPtr->m_data);
		}
	}


	void UpdateTransformDataRenderCommand::Destroy(void* cmdData)
	{
		UpdateTransformDataRenderCommand* cmdPtr = reinterpret_cast<UpdateTransformDataRenderCommand*>(cmdData);
		cmdPtr->~UpdateTransformDataRenderCommand();
	}
}