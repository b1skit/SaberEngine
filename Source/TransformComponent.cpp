// © 2023 Adam Badke. All rights reserved.
#include "CoreEngine.h"
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "RenderDataManager.h"
#include "RenderManager.h"
#include "TransformComponent.h"


namespace fr
{
	TransformComponent& TransformComponent::AttachTransformComponent(
		fr::EntityManager& em, entt::entity entity, fr::Transform* parent)
	{
		em.EmplaceComponent<fr::TransformComponent::NewIDMarker>(entity);
		
		// Note: We don't emplace a dirty marker; The Transform/TransformComponent currently track their dirty state

		return *em.EmplaceComponent<fr::TransformComponent>(entity, PrivateCTORTag{}, parent);
	}


	gr::Transform::RenderData TransformComponent::CreateRenderData(fr::TransformComponent& transformComponent)
	{
		fr::Transform& transform = transformComponent.GetTransform();
		return gr::Transform::RenderData{
			.g_model = transform.GetGlobalMatrix(),
			.g_transposeInvModel = glm::transpose(glm::inverse(transform.GetGlobalMatrix())),

			.m_globalPosition = transform.GetGlobalPosition(),
			.m_globalScale = transform.GetGlobalScale(),

			.m_globalRight = transform.GetGlobalRight(),
			.m_globalUp = transform.GetGlobalUp(),
			.m_globalForward = transform.GetGlobalForward(),

			.m_transformID = transformComponent.GetTransformID()
		};
	}


	void TransformComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity, uint64_t uniqueID)
	{
		if (ImGui::CollapsingHeader(std::format("Transform##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			fr::TransformComponent& transformCmpt = em.GetComponent<fr::TransformComponent>(owningEntity);

			transformCmpt.GetTransform().ShowImGuiWindow();

			ImGui::Unindent();
		}
	}


	TransformComponent::TransformComponent(PrivateCTORTag, fr::Transform* parent)
		: m_transform(parent)
	{
	}


	// ---


	void TransformComponent::DispatchTransformUpdateThreads(
		std::vector<std::future<void>>& taskFuturesOut, fr::Transform* rootNode)
	{
		// DFS walk down our Transform hierarchy, recomputing each Transform in turn. The goal here is to minimize the
		// (re)computation required when we copy Transforms for the Render thread

		taskFuturesOut.emplace_back(en::CoreEngine::GetThreadPool()->EnqueueJob(
			[rootNode]()
			{
				std::stack<fr::Transform*> transforms;
				transforms.push(rootNode);

				while (!transforms.empty())
				{
					fr::Transform* topTransform = transforms.top();
					transforms.pop();

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
		, m_data(fr::TransformComponent::CreateRenderData(transformComponent))
	{
	}


	void UpdateTransformDataRenderCommand::Execute(void* cmdData)
	{
		UpdateTransformDataRenderCommand* cmdPtr = reinterpret_cast<UpdateTransformDataRenderCommand*>(cmdData);

		gr::RenderDataManager& renderData = re::RenderManager::Get()->GetRenderDataManagerForModification();

		renderData.SetTransformData(cmdPtr->m_transformID, cmdPtr->m_data);
	}


	void UpdateTransformDataRenderCommand::Destroy(void* cmdData)
	{
		UpdateTransformDataRenderCommand* cmdPtr = reinterpret_cast<UpdateTransformDataRenderCommand*>(cmdData);
		cmdPtr->~UpdateTransformDataRenderCommand();
	}
}