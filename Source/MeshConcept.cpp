// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "EntityManager.h"
#include "ImGuiUtils.h"
#include "MeshConcept.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"


namespace fr
{
	entt::entity Mesh::AttachMeshConcept(entt::entity owningEntity, char const* name)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		SEAssert("A Mesh concept requires a Transform. The owningEntity should have this already",
			em.HasComponent<fr::TransformComponent>(owningEntity));
		
		entt::entity meshEntity = em.CreateEntity(name);

		em.EmplaceComponent<fr::Mesh::MeshConceptMarker>(meshEntity);

		fr::TransformComponent const& owningTransformCmpt = em.GetComponent<fr::TransformComponent>(owningEntity);

		gr::RenderDataComponent::AttachNewRenderDataComponent(em, meshEntity, owningTransformCmpt.GetTransformID());

		// Mesh bounds: Encompasses all attached primitive bounds
		fr::BoundsComponent::AttachBoundsComponent(em, meshEntity, fr::BoundsComponent::Contents::Mesh);

		fr::Relationship& meshRelationship = em.GetComponent<fr::Relationship>(meshEntity);
		meshRelationship.SetParent(em, owningEntity);

		return meshEntity;
	}


	void Mesh::ShowImGuiWindow(entt::entity meshConcept)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		fr::NameComponent const& meshName = em.GetComponent<fr::NameComponent>(meshConcept);
		fr::Relationship const& meshRelationship = em.GetComponent<fr::Relationship>(meshConcept);
		fr::TransformComponent& owningTransform =
			*em.GetFirstInHierarchyAbove<fr::TransformComponent>(meshRelationship.GetParent());

		if (ImGui::CollapsingHeader(
			std::format("{}##{}", meshName.GetName(), meshName.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			if (ImGui::CollapsingHeader(
				std::format("Transform:##{}", meshName.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				//m_ownerTransform->ShowImGuiWindow();
				// ECS_CONVERSION: RESTORE THIS BEHAVIOR
				ImGui::Unindent();
			}

			if (ImGui::CollapsingHeader(
				std::format("Mesh Bounds:##{}", meshName.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				// ECS_CONVERSION: RESTORE THIS BEHAVIOR
				//m_localBounds.ShowImGuiWindow();
				ImGui::Unindent();
			}

			if (ImGui::CollapsingHeader(
				std::format("Mesh Primitives:##{}", meshName.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();
				
				entt::entity curChild = meshRelationship.GetFirstChild();
				do
				{
					fr::MeshPrimitiveComponent& meshPrimCmpt = em.GetComponent<fr::MeshPrimitiveComponent>(curChild);

					meshPrimCmpt.ShowImGuiWindow(curChild);

					fr::Relationship const& childRelationship = em.GetComponent<fr::Relationship>(curChild);
					curChild = childRelationship.GetNext();
				} while (curChild != meshRelationship.GetFirstChild());

				ImGui::Unindent();
			}

			ImGui::Unindent();
		}
	}
}