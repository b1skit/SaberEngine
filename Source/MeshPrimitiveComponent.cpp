// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "MaterialComponent.h"
#include "MeshPrimitive.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"
#include "VertexStream.h"


namespace
{
	void AttachMeshPrimitiveComponentHelper(
		fr::EntityManager& em,
		entt::entity owningEntity,
		gr::MeshPrimitive const* meshPrimitive,
		gr::RenderDataComponent& meshPrimRenderCmpt,
		glm::vec3 const& positionMinXYZ,
		glm::vec3 const& positionMaxXYZ)
	{
		// MeshPrimitive:
		em.EmplaceComponent<fr::MeshPrimitiveComponent>(
			owningEntity,
			fr::MeshPrimitiveComponent{
				.m_meshPrimitive = meshPrimitive
			});

		// Bounds for the MeshPrimitive
		fr::BoundsComponent::AttachBoundsComponent(
			em,
			owningEntity,
			positionMinXYZ,
			positionMaxXYZ,
			reinterpret_cast<std::vector<glm::vec3> const&>(
				meshPrimitive->GetVertexStream(gr::MeshPrimitive::Slot::Position)->GetDataAsVector()));
		fr::BoundsComponent const& meshPrimitiveBounds = em.GetComponent<fr::BoundsComponent>(owningEntity);

		fr::Relationship& owningEntityRelationship = em.GetComponent<fr::Relationship>(owningEntity);

		// If there's a BoundsComponent in the heirarchy above (i.e. from a Mesh), assume it's encapsulating the
		// MeshPrimitive:
		if (owningEntityRelationship.HasParent())
		{
			entt::entity nextEntity = entt::null;
			fr::BoundsComponent* encapsulatingBounds = em.GetFirstAndEntityInHierarchyAbove<fr::BoundsComponent>(
				owningEntityRelationship.GetParent(),
				nextEntity);
			if (encapsulatingBounds != nullptr)
			{
				encapsulatingBounds->ExpandBoundsHierarchy(em, meshPrimitiveBounds, nextEntity);
			}
		}

		// Mark our new MeshPrimitive as dirty:
		em.EmplaceComponent<DirtyMarker<fr::MeshPrimitiveComponent>>(owningEntity);
	}
}


namespace fr
{
	entt::entity MeshPrimitiveComponent::CreateMeshPrimitiveConcept(
		fr::EntityManager& em,
		entt::entity owningEntity, 
		gr::MeshPrimitive const* meshPrimitive, 
		glm::vec3 const& positionMinXYZ,
		glm::vec3 const& positionMaxXYZ)
	{
		SEAssert("A MeshPrimitive's owningEntity requires a TransformComponent",
			em.HasComponent<fr::TransformComponent>(owningEntity));
		SEAssert("A MeshPrimitive's owningEntity requires a RenderDataComponent",
			em.HasComponent<gr::RenderDataComponent>(owningEntity));

		entt::entity meshPrimitiveConcept = em.CreateEntity(meshPrimitive->GetName());

		// Relationship:
		fr::Relationship& meshPrimitiveRelationship = em.GetComponent<fr::Relationship>(meshPrimitiveConcept);
		meshPrimitiveRelationship.SetParent(em, owningEntity);

		// RenderDataComponent: A MeshPrimitive has its own RenderDataID, but shares the TransformID of its owningEntity
		fr::TransformComponent const& transformComponent = em.GetComponent<fr::TransformComponent>(owningEntity);

		gr::RenderDataComponent& meshPrimRenderCmpt = gr::RenderDataComponent::AttachNewRenderDataComponent(
			em, meshPrimitiveConcept, transformComponent.GetTransformID());

		AttachMeshPrimitiveComponentHelper(
			em, meshPrimitiveConcept, meshPrimitive, meshPrimRenderCmpt, positionMinXYZ, positionMaxXYZ);

		// Set the mesh primitive bounds feature bit for the culling system
		meshPrimRenderCmpt.SetFeatureBit(gr::RenderObjectFeature::IsMeshPrimitiveBounds);

		return meshPrimitiveConcept; // Note: A Material component must be attached to the returned entity
	}


	void MeshPrimitiveComponent::AttachMeshPrimitiveComponent(
		fr::EntityManager& em,
		entt::entity owningEntity,
		gr::MeshPrimitive const* meshPrimitive,
		glm::vec3 const& positionMinXYZ /*= fr::BoundsComponent::k_invalidMinXYZ*/, // Default: Compute bounds manually
		glm::vec3 const& positionMaxXYZ /*= fr::BoundsComponent::k_invalidMaxXYZ*/) // Default: Compute bounds manually
	{
		SEAssert("A MeshPrimitive's owningEntity requires a TransformComponent",
			em.HasComponent<fr::TransformComponent>(owningEntity));
		SEAssert("A MeshPrimitive's owningEntity requires a RenderDataComponent",
			em.HasComponent<gr::RenderDataComponent>(owningEntity));

		gr::RenderDataComponent* meshPrimRenderCmpt = 
			em.GetFirstInHierarchyAbove<gr::RenderDataComponent>(owningEntity);

		// Note: A Material component will typically need to be attached to the owningEntity
		AttachMeshPrimitiveComponentHelper(
			em, owningEntity, meshPrimitive, *meshPrimRenderCmpt, positionMinXYZ, positionMaxXYZ);
	}


	MeshPrimitiveComponent& MeshPrimitiveComponent::AttachRawMeshPrimitiveConcept(
		EntityManager& em,
		entt::entity owningEntity, 
		gr::RenderDataComponent const& sharedRenderDataCmpt, 
		gr::MeshPrimitive const* meshPrimitive)
	{
		// MeshPrimitive:
		MeshPrimitiveComponent& meshPrimCmpt = *em.EmplaceComponent<MeshPrimitiveComponent>(
			owningEntity,
			MeshPrimitiveComponent
			{
				.m_meshPrimitive = meshPrimitive
			});

		// Mark our new MeshPrimitive as dirty:
		em.EmplaceComponent<DirtyMarker<fr::MeshPrimitiveComponent>>(owningEntity);

		return meshPrimCmpt;
	}


	gr::MeshPrimitive::RenderData MeshPrimitiveComponent::CreateRenderData(
		MeshPrimitiveComponent const& meshPrimitiveComponent, fr::NameComponent const&)
	{
		gr::MeshPrimitive::RenderData renderData = gr::MeshPrimitive::RenderData{
			.m_meshPrimitiveParams = meshPrimitiveComponent.m_meshPrimitive->GetMeshParams(),
			// Vertex streams copied below...
			.m_indexStream = meshPrimitiveComponent.m_meshPrimitive->GetIndexStream(),
			.m_dataHash = meshPrimitiveComponent.m_meshPrimitive->GetDataHash()
		};

		std::vector<re::VertexStream const*> const& vertexStreams =
			meshPrimitiveComponent.m_meshPrimitive->GetVertexStreams();
		for (size_t slotIdx = 0; slotIdx < vertexStreams.size(); slotIdx++)
		{
			renderData.m_vertexStreams[slotIdx] = vertexStreams[slotIdx];
		}

		return renderData;
	}


	void MeshPrimitiveComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity meshPrimitive)
	{
		fr::NameComponent const& nameCmpt = em.GetComponent<fr::NameComponent>(meshPrimitive);

		if (ImGui::CollapsingHeader(
			std::format("{}##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			
			// RenderDataComponent:
			gr::RenderDataComponent::ShowImGuiWindow(em, meshPrimitive);

			fr::MeshPrimitiveComponent& meshPrimCmpt = em.GetComponent<fr::MeshPrimitiveComponent>(meshPrimitive);
			meshPrimCmpt.m_meshPrimitive->ShowImGuiWindow();

			// Material:
			fr::MaterialComponent* matCmpt = em.TryGetComponent<fr::MaterialComponent>(meshPrimitive);
			if (matCmpt)
			{
				fr::MaterialComponent::ShowImGuiWindow(em, meshPrimitive);
			}
			else
			{
				ImGui::Indent();
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "<no material>"); // e.g. deferred mesh
				ImGui::Unindent();
			}

			// Bounds
			fr::BoundsComponent::ShowImGuiWindow(em, meshPrimitive);

			// Transform:
			entt::entity transformOwner = entt::null;
			fr::TransformComponent* transformComponent =
				em.GetFirstAndEntityInHierarchyAbove<fr::TransformComponent>(meshPrimitive, transformOwner);
			fr::TransformComponent::ShowImGuiWindow(em, transformOwner, static_cast<uint64_t>(meshPrimitive));

			ImGui::Unindent();
		}
	}
}