// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"

#include "Core/ThreadPool.h"

#include "Renderer/RenderDataManager.h"


namespace pr
{
	TransformComponent& TransformComponent::AttachTransformComponent(pr::EntityManager& em, entt::entity entity)
	{
		em.EmplaceComponent<pr::TransformComponent::NewIDMarker>(entity);
		
		// Retrieve the parent transform, if one exists:
		pr::Relationship const& relationship = em.GetComponent<pr::Relationship>(entity);
		TransformComponent* parentTransformCmpt = relationship.GetFirstInHierarchyAbove<TransformComponent>(em);

		pr::Transform* parentTransform = nullptr;
		if (parentTransformCmpt)
		{
			parentTransform = &parentTransformCmpt->GetTransform();
		}

		// Attach our TransformComponent:
		TransformComponent& transformCmpt = 
			*em.EmplaceComponent<pr::TransformComponent>(entity, PrivateCTORTag{}, parentTransform);
		
		// A Transform must be associated with a RenderDataID; Attach a RenderDataComponent if one doesn't already exist
		pr::RenderDataComponent::GetCreateRenderDataComponent(em, entity, transformCmpt.GetTransformID());

		// Note: We don't emplace a dirty marker; The Transform/TransformComponent currently track their dirty state
		return transformCmpt;
	}


	gr::Transform::RenderData TransformComponent::CreateRenderData(
		pr::EntityManager& em, pr::TransformComponent& transformComponent)
	{
		pr::Transform& transform = transformComponent.GetTransform();

		gr::TransformID parentTransformID = gr::k_invalidTransformID;
		pr::Transform const* parentTransform = transform.GetParent();
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


	void TransformComponent::ShowImGuiWindow(pr::EntityManager& em, entt::entity owningEntity, uint64_t uniqueID)
	{
		if (ImGui::CollapsingHeader(std::format("Transform##{}", uniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			if (pr::TransformComponent* transformCmpt = em.TryGetComponent<pr::TransformComponent>(owningEntity))
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


	TransformComponent::TransformComponent(PrivateCTORTag, pr::Transform* parent)
		: m_transform(parent)
	{
	}


	// ---


	void TransformComponent::DispatchTransformUpdateThreads(
		std::vector<std::future<void>>& taskFuturesOut, pr::Transform* rootNode)
	{
		// DFS walk down our Transform hierarchy, recomputing each Transform in turn. The goal here is to minimize the
		// (re)computation required when we copy Transforms for the Render thread

		taskFuturesOut.emplace_back(core::ThreadPool::EnqueueJob(
			[rootNode]()
			{
				std::stack<pr::Transform*> transforms;
				transforms.push(rootNode);

				bool parentChanged = false;

				while (!transforms.empty())
				{
					pr::Transform* topTransform = transforms.top();
					transforms.pop();

					parentChanged |= topTransform->Recompute(parentChanged);

					for (pr::Transform* child : topTransform->GetChildren())
					{
						transforms.push(child);
					}
				}
			}));
	}


	// ---


	UpdateTransformDataRenderCommand::UpdateTransformDataRenderCommand(
		pr::EntityManager& em, pr::TransformComponent& transformComponent)
		: m_transformID(transformComponent.GetTransformID())
		, m_data(pr::TransformComponent::CreateRenderData(em, transformComponent))
	{
	}


	void UpdateTransformDataRenderCommand::Execute(void* cmdData)
	{
		UpdateTransformDataRenderCommand* cmdPtr = reinterpret_cast<UpdateTransformDataRenderCommand*>(cmdData);

		gr::RenderDataManager& renderData = cmdPtr->GetRenderDataManagerForModification();

		renderData.SetTransformData(cmdPtr->m_transformID, cmdPtr->m_data);
	}
}