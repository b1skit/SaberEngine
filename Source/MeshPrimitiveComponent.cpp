// © 2023 Adam Badke. All rights reserved.
#include "GameplayManager.h"
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
	gr::MeshPrimitive::RenderData MeshPrimitive::CreateRenderData(MeshPrimitiveComponent const& meshPrimitiveComponent)
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


	entt::entity MeshPrimitive::AttachMeshPrimitiveConcept(
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

		return AttachMeshPrimitiveConcept(owningEntity, meshPrimitiveSceneData.get(), positionMinXYZ, positionMaxXYZ);
	}


	entt::entity MeshPrimitive::AttachMeshPrimitiveConcept(
		entt::entity owningEntity, 
		gr::MeshPrimitive const* meshPrimitive, 
		glm::vec3 const& positionMinXYZ,
		glm::vec3 const& positionMaxXYZ)
	{
		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		SEAssert("A mesh primitive's owning entity requires a Relationship component",
			gpm.HasComponent<fr::Relationship>(owningEntity));
		SEAssert("A mesh primitive's owning entity requires a TransformComponent",
			fr::Relationship::IsInHierarchyAbove<fr::TransformComponent>(owningEntity));

		entt::entity meshPrimitiveEntity = gpm.CreateEntity(meshPrimitive->GetName());

		// Relationship:
		fr::Relationship& meshPrimitiveRelationship =
			fr::Relationship::AttachRelationshipComponent(gpm, meshPrimitiveEntity);
		meshPrimitiveRelationship.SetParent(gpm, owningEntity);

		// RenderDataComponent:
		fr::TransformComponent const* transformComponent =
			fr::Relationship::GetFirstInHierarchyAbove<fr::TransformComponent>(owningEntity);
		gr::RenderDataComponent::AttachNewRenderDataComponent(
			gpm, meshPrimitiveEntity, transformComponent->GetTransformID());

		// MeshPrimitive:
		gpm.EmplaceComponent<fr::MeshPrimitive::MeshPrimitiveComponent>(
			meshPrimitiveEntity,
			MeshPrimitiveComponent{
				.m_meshPrimitive = meshPrimitive
			});
		
		// Bounds for the MeshPrimitive
		fr::BoundsComponent::AttachBoundsComponent(
			gpm,
			meshPrimitiveEntity,
			positionMinXYZ,
			positionMaxXYZ,
			reinterpret_cast<std::vector<glm::vec3> const&>(
				meshPrimitive->GetVertexStream(gr::MeshPrimitive::Slot::Position)->GetDataAsVector()));
		fr::BoundsComponent const& meshPrimitiveBounds = gpm.GetComponent<fr::BoundsComponent>(meshPrimitiveEntity);

		// If there is a BoundsComponent in the heirarchy above, assume it's encapsulating the MeshPrimitive:
		entt::entity nextEntity = entt::null;
		fr::BoundsComponent* encapsulatingBounds =
			fr::Relationship::GetFirstAndEntityInHierarchyAbove<fr::BoundsComponent>(
				meshPrimitiveRelationship.GetParent(), nextEntity);
		if (encapsulatingBounds != nullptr)
		{
			encapsulatingBounds->ExpandBoundsHierarchy(meshPrimitiveBounds, nextEntity);
		}

		// Mark our new MeshPrimitive as dirty:
		gpm.EmplaceComponent<DirtyMarker<fr::MeshPrimitive::MeshPrimitiveComponent>>(meshPrimitiveEntity);

		// Note: A Material component must be attached to the returned entity
		return meshPrimitiveEntity;
	}


	MeshPrimitive::MeshPrimitiveComponent& MeshPrimitive::AttachRawMeshPrimitiveConcept(
		GameplayManager& gpm,
		entt::entity owningEntity, 
		gr::RenderDataComponent const& sharedRenderDataCmpt, 
		gr::MeshPrimitive const* meshPrimitive)
	{
		SEAssert("A mesh primitive's owning entity requires a Relationship component",
			gpm.HasComponent<fr::Relationship>(owningEntity));

		entt::entity meshPrimitiveEntity = gpm.CreateEntity(meshPrimitive->GetName());

		// Relationship:
		fr::Relationship& meshPrimitiveRelationship =
			fr::Relationship::AttachRelationshipComponent(gpm, meshPrimitiveEntity);
		meshPrimitiveRelationship.SetParent(gpm, owningEntity);

		// Shared RenderDataComponent:
		gr::RenderDataComponent::AttachSharedRenderDataComponent(gpm, meshPrimitiveEntity, sharedRenderDataCmpt);

		// MeshPrimitive:
		MeshPrimitive::MeshPrimitiveComponent& meshPrimCmpt = *gpm.EmplaceComponent<fr::MeshPrimitive::MeshPrimitiveComponent>(
			owningEntity,
			MeshPrimitiveComponent{
				.m_meshPrimitive = meshPrimitive
			});

		// Mark our new MeshPrimitive as dirty:
		gpm.EmplaceComponent<DirtyMarker<fr::MeshPrimitive::MeshPrimitiveComponent>>(meshPrimitiveEntity);

		return meshPrimCmpt;
	}
}