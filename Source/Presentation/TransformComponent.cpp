// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"

#include "Core/ThreadPool.h"

#include "Renderer/RenderDataManager.h"
#include "Renderer/RenderManager.h"


namespace fr
{
	TransformComponent& TransformComponent::AttachTransformComponent(fr::EntityManager& em, entt::entity entity)
	{
		em.EmplaceComponent<fr::TransformComponent::NewIDMarker>(entity);
		
		// Retrieve the parent transform, if one exists:
		fr::Relationship const& relationship = em.GetComponent<fr::Relationship>(entity);
		TransformComponent* parentTransformCmpt = relationship.GetFirstInHierarchyAbove<TransformComponent>();

		fr::Transform* parentTransform = nullptr;
		if (parentTransformCmpt)
		{
			parentTransform = &parentTransformCmpt->GetTransform();
		}

		// Attach our TransformComponent:
		TransformComponent& transformCmpt = 
			*em.EmplaceComponent<fr::TransformComponent>(entity, PrivateCTORTag{}, parentTransform);
		
		// A Transform must be associated with a RenderDataID; Attach a RenderDataComponent if one doesn't already exist
		fr::RenderDataComponent::GetCreateRenderDataComponent(em, entity, transformCmpt.GetTransformID());

		// Note: We don't emplace a dirty marker; The Transform/TransformComponent currently track their dirty state
		return transformCmpt;
	}


	gr::Transform::RenderData TransformComponent::CreateRenderData(fr::TransformComponent& transformComponent)
	{
		fr::Transform& transform = transformComponent.GetTransform();

		gr::TransformID parentTransformID = gr::k_invalidTransformID;
		fr::Transform const* parentTransform = transform.GetParent();
		if (parentTransform)
		{
			parentTransformID = parentTransform->GetTransformID();
		}

		return gr::Transform::RenderData{
			.g_model = transform.GetGlobalMatrix(),
			.g_transposeInvModel = glm::transpose(glm::inverse(transform.GetGlobalMatrix())),

			.g_local = transform.GetLocalMatrix(),

			.m_globalPosition = transform.GetGlobalTranslation(),
			.m_globalScale = transform.GetGlobalScale(),

			.m_globalRight = transform.GetGlobalRight(),
			.m_globalUp = transform.GetGlobalUp(),
			.m_globalForward = transform.GetGlobalForward(),

			.m_transformID = transformComponent.GetTransformID(),
			.m_parentTransformID = parentTransformID,
		};
	}


	void TransformComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity, uint64_t uniqueID)
	{
		if (ImGui::CollapsingHeader(std::format("Transform##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			if (fr::TransformComponent* transformCmpt = em.TryGetComponent<fr::TransformComponent>(owningEntity))
			{
				transformCmpt->GetTransform().ShowImGuiWindow(em, owningEntity);
			}
			else
			{
				ImGui::Text("<No transform component attached>");
			}

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

		taskFuturesOut.emplace_back(core::ThreadPool::Get()->EnqueueJob(
			[rootNode]()
			{
				std::stack<fr::Transform*> transforms;
				transforms.push(rootNode);

				bool parentChanged = false;

				while (!transforms.empty())
				{
					fr::Transform* topTransform = transforms.top();
					transforms.pop();

					parentChanged |= topTransform->Recompute(parentChanged);

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

		gr::RenderDataManager& renderData = re::RenderManager::Get()->GetContext()->GetRenderDataManagerForModification();

		renderData.SetTransformData(cmdPtr->m_transformID, cmdPtr->m_data);
	}


	void UpdateTransformDataRenderCommand::Destroy(void* cmdData)
	{
		UpdateTransformDataRenderCommand* cmdPtr = reinterpret_cast<UpdateTransformDataRenderCommand*>(cmdData);
		cmdPtr->~UpdateTransformDataRenderCommand();
	}
}