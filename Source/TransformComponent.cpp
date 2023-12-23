// © 2023 Adam Badke. All rights reserved.
#include "CoreEngine.h"
#include "GameplayManager.h"
#include "RenderDataManager.h"
#include "RenderManager.h"
#include "TransformComponent.h"


namespace fr
{
	// gr::k_sharedIdentityTransformID == 0, so we start at 1
	std::atomic<gr::TransformID> TransformComponent::s_transformIDs = gr::k_sharedIdentityTransformID + 1;


	gr::Transform::RenderData TransformComponent::CreateRenderData(fr::Transform& transform)
	{
		return gr::Transform::RenderData{
			.g_model = transform.GetGlobalMatrix(),
			.g_transposeInvModel = glm::transpose(glm::inverse(transform.GetGlobalMatrix()))
		};
	}


	TransformComponent& TransformComponent::AttachTransformComponent(
		fr::GameplayManager& gpm, entt::entity entity, fr::Transform* parent)
	{
		gpm.EmplaceComponent<fr::TransformComponent::NewIDMarker>(entity);
		return *gpm.EmplaceComponent<fr::TransformComponent>(entity, PrivateCTORTag{}, parent);
	}


	TransformComponent::TransformComponent(PrivateCTORTag, fr::Transform* parent)
		: m_transformID(s_transformIDs.fetch_add(1))
		, m_transform(parent)
	{
	}


	// ---


	void TransformComponent::DispatchTransformUpdateThread(
		std::vector<std::future<void>>& taskFuturesOut, fr::Transform* rootNode)
	{
		// DFS walk down our Transform hierarchy, recomputing each Transform in turn. The goal here is to minimize the
		// (re)computation required when we copy Transforms for the Render thread

		taskFuturesOut.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
			[rootNode]()
			{
				fr::GameplayManager& gpm = *fr::GameplayManager::Get();

				std::stack<fr::Transform*> transforms;
				transforms.push(rootNode);

				while (!transforms.empty())
				{
					fr::Transform* topTransform = transforms.top();
					transforms.pop();

					topTransform->ClearHasChangedFlag();
					topTransform->Recompute();

					for (fr::Transform* child : topTransform->GetChildren())
					{
						transforms.push(child);
					}
				}
			}));
	}


	// ---


	UpdateTransformDataRenderCommand::UpdateTransformDataRenderCommand(fr::TransformComponent& transformComponent)
		: m_transformID(transformComponent.GetTransformID())
		, m_data(fr::TransformComponent::CreateRenderData(transformComponent.GetTransform()))
	{
	}


	void UpdateTransformDataRenderCommand::Execute(void* cmdData)
	{
		std::vector<std::unique_ptr<re::RenderSystem>> const& renderSystems =
			re::RenderManager::Get()->GetRenderSystems();

		UpdateTransformDataRenderCommand* cmdPtr = reinterpret_cast<UpdateTransformDataRenderCommand*>(cmdData);

		for (size_t renderSystemIdx = 0; renderSystemIdx < renderSystems.size(); renderSystemIdx++)
		{
			gr::RenderDataManager& renderData =
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