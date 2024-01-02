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

		gr::RenderDataComponent& meshRenderData = 
			gr::RenderDataComponent::AttachNewRenderDataComponent(em, meshEntity, owningTransformCmpt.GetTransformID());

		// Mesh bounds: Encompasses all attached primitive bounds
		fr::BoundsComponent::AttachBoundsComponent(em, meshEntity);

		// Mark our RenderDataComponent so the renderer can differentiate between Mesh and MeshPrimitive Bounds
		meshRenderData.SetFeature(gr::RenderObjectFeature::IsMeshBounds);

		fr::Relationship& meshRelationship = em.GetComponent<fr::Relationship>(meshEntity);
		meshRelationship.SetParent(em, owningEntity);

		return meshEntity;
	}


	void Mesh::ShowImGuiWindow(fr::EntityManager& em, entt::entity meshConcept)
	{
		fr::NameComponent const& meshName = em.GetComponent<fr::NameComponent>(meshConcept);

		if (ImGui::CollapsingHeader(
			std::format("{}##{}", meshName.GetName(), meshName.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			gr::RenderDataComponent::ShowImGuiWindow(em, meshConcept);

			fr::Relationship const& meshRelationship = em.GetComponent<fr::Relationship>(meshConcept);

			entt::entity owningTransformEntity = entt::null;
			fr::TransformComponent& owningTransform =
				*em.GetFirstAndEntityInHierarchyAbove<fr::TransformComponent>(meshRelationship.GetParent(), owningTransformEntity);

			// Transform:
			fr::TransformComponent::TransformComponent::ShowImGuiWindow(
				em, owningTransformEntity, static_cast<uint64_t>(meshConcept));

			// Bounds:
			fr::BoundsComponent::ShowImGuiWindow(em, meshConcept);

			// Mesh primitives:
			if (ImGui::CollapsingHeader(
				std::format("Mesh Primitives:##{}", meshName.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
			{
				ImGui::Indent();

				entt::entity curChild = meshRelationship.GetFirstChild();
				do
				{
					fr::MeshPrimitiveComponent& meshPrimCmpt = em.GetComponent<fr::MeshPrimitiveComponent>(curChild);

					meshPrimCmpt.ShowImGuiWindow(em, curChild);

					fr::Relationship const& childRelationship = em.GetComponent<fr::Relationship>(curChild);
					curChild = childRelationship.GetNext();
				} while (curChild != meshRelationship.GetFirstChild());

				ImGui::Unindent();
			}

			ImGui::Unindent();
		}
	}
}