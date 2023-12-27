// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "MeshPrimitive.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"
#include "VertexStream.h"


namespace fr
{
	entt::entity MeshPrimitiveComponent::AttachMeshPrimitiveConcept(
		fr::EntityManager& em,
		entt::entity owningEntity,
		char const* name,
		std::vector<uint32_t>* indices,
		std::vector<float>& positions,
		glm::vec3 const& positionMinXYZ, // Pass fr::BoundsConcept::k_invalidMinXYZ to compute bounds manually
		glm::vec3 const& positionMaxXYZ, // Pass fr::BoundsConcept::k_invalidMaxXYZ to compute bounds manually
		std::vector<float>* normals,
		std::vector<float>* tangents,
		std::vector<float>* uv0,
		std::vector<float>* colors,
		std::vector<uint8_t>* joints,
		std::vector<float>* weights,
		gr::MeshPrimitive::MeshPrimitiveParams const& meshParams)
	{
		std::shared_ptr<gr::MeshPrimitive> meshPrimitiveSceneData = gr::MeshPrimitive::Create(
			name,
			indices,
			positions,
			normals,
			tangents,
			uv0,
			colors,
			joints,
			weights,
			meshParams);

		return AttachMeshPrimitiveConcept(em, owningEntity, meshPrimitiveSceneData.get(), positionMinXYZ, positionMaxXYZ);
	}


	entt::entity MeshPrimitiveComponent::AttachMeshPrimitiveConcept(
		fr::EntityManager& em,
		entt::entity owningEntity, 
		gr::MeshPrimitive const* meshPrimitive, 
		glm::vec3 const& positionMinXYZ,
		glm::vec3 const& positionMaxXYZ)
	{
		SEAssert("A mesh primitive's owning entity requires a TransformComponent",
			em.IsInHierarchyAbove<fr::TransformComponent>(owningEntity));

		entt::entity meshPrimitiveEntity = em.CreateEntity(meshPrimitive->GetName());

		// Relationship:
		fr::Relationship& meshPrimitiveRelationship = em.GetComponent<fr::Relationship>(meshPrimitiveEntity);
		meshPrimitiveRelationship.SetParent(em, owningEntity);

		// RenderDataComponent:
		fr::TransformComponent const* transformComponent =
			em.GetFirstInHierarchyAbove<fr::TransformComponent>(owningEntity);
		gr::RenderDataComponent::AttachNewRenderDataComponent(
			em, meshPrimitiveEntity, transformComponent->GetTransformID());

		// MeshPrimitive:
		em.EmplaceComponent<fr::MeshPrimitiveComponent>(
			meshPrimitiveEntity,
			MeshPrimitiveComponent{
				.m_meshPrimitive = meshPrimitive
			});
		
		// Bounds for the MeshPrimitive
		fr::BoundsComponent::AttachBoundsComponent(
			em,
			meshPrimitiveEntity,
			positionMinXYZ,
			positionMaxXYZ,
			reinterpret_cast<std::vector<glm::vec3> const&>(
				meshPrimitive->GetVertexStream(gr::MeshPrimitive::Slot::Position)->GetDataAsVector()),
			fr::BoundsComponent::Contents::MeshPrimitive);
		fr::BoundsComponent const& meshPrimitiveBounds = em.GetComponent<fr::BoundsComponent>(meshPrimitiveEntity);

		// If there is a BoundsComponent in the heirarchy above, assume it's encapsulating the MeshPrimitive:
		entt::entity nextEntity = entt::null;
		fr::BoundsComponent* encapsulatingBounds = em.GetFirstAndEntityInHierarchyAbove<fr::BoundsComponent>(
			meshPrimitiveRelationship.GetParent(), 
			nextEntity);
		if (encapsulatingBounds != nullptr)
		{
			encapsulatingBounds->ExpandBoundsHierarchy(em, meshPrimitiveBounds, nextEntity);
		}

		// Mark our new MeshPrimitive as dirty:
		em.EmplaceComponent<DirtyMarker<fr::MeshPrimitiveComponent>>(meshPrimitiveEntity);

		// Note: A Material component must be attached to the returned entity
		return meshPrimitiveEntity;
	}


	MeshPrimitiveComponent& MeshPrimitiveComponent::AttachRawMeshPrimitiveConcept(
		EntityManager& em,
		entt::entity owningEntity, 
		gr::RenderDataComponent const& sharedRenderDataCmpt, 
		gr::MeshPrimitive const* meshPrimitive)
	{
		entt::entity meshPrimitiveEntity = em.CreateEntity(meshPrimitive->GetName());

		// Relationship:
		fr::Relationship& meshPrimitiveRelationship = em.GetComponent<fr::Relationship>(meshPrimitiveEntity);
		meshPrimitiveRelationship.SetParent(em, owningEntity);

		// Shared RenderDataComponent:
		gr::RenderDataComponent::AttachSharedRenderDataComponent(em, meshPrimitiveEntity, sharedRenderDataCmpt);

		// MeshPrimitive:
		MeshPrimitiveComponent& meshPrimCmpt = *em.EmplaceComponent<MeshPrimitiveComponent>(
			meshPrimitiveEntity,
			MeshPrimitiveComponent
			{
				.m_meshPrimitive = meshPrimitive
			});

		// Mark our new MeshPrimitive as dirty:
		em.EmplaceComponent<DirtyMarker<fr::MeshPrimitiveComponent>>(meshPrimitiveEntity);

		return meshPrimCmpt;
	}


	gr::MeshPrimitive::RenderData MeshPrimitiveComponent::CreateRenderData(
		MeshPrimitiveComponent const& meshPrimitiveComponent)
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
}