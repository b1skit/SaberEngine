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
		entt::entity meshConcept,
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
		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		SEAssert("A mesh primitive's owning mesh concept requires a Relationship component",
			gpm.HasComponent<fr::Relationship>(meshConcept));
		SEAssert("A mesh primitive's owning mesh concept requires a TransformComponent",
			fr::Relationship::HasComponentInParentHierarchy<fr::TransformComponent>(meshConcept));
		SEAssert("A mesh primitive's owning mesh concept requires a Bounds component",
			gpm.HasComponent<fr::Bounds>(meshConcept));
		SEAssert("A mesh primitive's owning mesh concept requires a NameComponent",
			gpm.HasComponent<fr::NameComponent>(meshConcept));

		entt::entity meshPrimitiveEntity = gpm.CreateEntity(name);

		// Each MeshPrimitive is an entity related to a Mesh concept via a Relationship:
		fr::Relationship& meshRelationship = fr::Relationship::AttachRelationshipComponent(gpm, meshPrimitiveEntity);
		meshRelationship.SetParent(gpm, meshConcept);

		// Attach a new RenderDataComponent:
		fr::TransformComponent const* transformComponent =
			fr::Relationship::GetComponentInHierarchyAbove<fr::TransformComponent>(meshConcept);

		gr::RenderDataComponent::AttachNewRenderDataComponent(
			gpm, meshPrimitiveEntity, transformComponent->GetTransformID());


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

		// Attach a MeshPrimitive object:
		gpm.EmplaceComponent<fr::MeshPrimitive::MeshPrimitiveComponent>(
			meshPrimitiveEntity,
			MeshPrimitiveComponent{
				.m_meshPrimitive = meshPrimitiveSceneData.get()
			});

		// Attach a primitive bounds
		fr::Bounds::AttachBoundsComponent(
			gpm,
			meshPrimitiveEntity,
			positionMinXYZ,
			positionMaxXYZ,
			reinterpret_cast<std::vector<glm::vec3> const&>(positions));
		fr::Bounds const& meshPrimitiveBounds = gpm.GetComponent<fr::Bounds>(meshPrimitiveEntity);

		// Update the Bounds of the MeshConcept:
		fr::Bounds& meshConceptBounds = gpm.GetComponent<fr::Bounds>(meshConcept);
		meshConceptBounds.ExpandBounds(meshPrimitiveBounds);

		// Mark our new MeshPrimitive as dirty:
		gpm.EmplaceComponent<DirtyMarker<fr::MeshPrimitive::MeshPrimitiveComponent>>(meshPrimitiveEntity);

		// Note: A Material component must be attached to the returned entity
		return meshPrimitiveEntity;
	}
}