// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "MaterialInstanceComponent.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "SkinningComponent.h"
#include "TransformComponent.h"

#include "Renderer/MeshPrimitive.h"
#include "Renderer/VertexStream.h"


namespace
{
	void AttachMeshPrimitiveComponentHelper(
		fr::EntityManager& em,
		entt::entity owningEntity,
		gr::MeshPrimitive const* meshPrimitive,
		fr::RenderDataComponent& meshPrimRenderCmpt,
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
			positionMaxXYZ);

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
		SEAssert(em.HasComponent<fr::RenderDataComponent>(owningEntity),
			"A MeshPrimitive's owningEntity requires a RenderDataComponent");

		entt::entity meshPrimitiveConcept = em.CreateEntity(meshPrimitive->GetName());

		// Relationship:
		fr::Relationship& meshPrimitiveRelationship = em.GetComponent<fr::Relationship>(meshPrimitiveConcept);
		meshPrimitiveRelationship.SetParent(em, owningEntity);

		// RenderDataComponent: A MeshPrimitive has its own RenderDataID, but shares the TransformID of its owningEntity
		// However, if the owning entity does not have a TransformComponent, we attach one to the meshPrimitiveConcept
		// entity instead (as it's possible the owning entity is associated with a shared TransformID, without having
		// the TransformComponent attached to it)
		fr::TransformComponent const* transformComponent = em.TryGetComponent<fr::TransformComponent>(owningEntity);
		if (!transformComponent)
		{
			transformComponent = &fr::TransformComponent::AttachTransformComponent(em, meshPrimitiveConcept);
		}

		fr::RenderDataComponent* meshPrimRenderCmpt = fr::RenderDataComponent::GetCreateRenderDataComponent(
			em, meshPrimitiveConcept, transformComponent->GetTransformID());

		meshPrimRenderCmpt->SetFeatureBit(gr::RenderObjectFeature::IsMeshPrimitive);

		AttachMeshPrimitiveComponentHelper(
			em, meshPrimitiveConcept, meshPrimitive, *meshPrimRenderCmpt, positionMinXYZ, positionMaxXYZ);

		// Set the mesh primitive bounds feature bit for the culling system
		meshPrimRenderCmpt->SetFeatureBit(gr::RenderObjectFeature::IsMeshPrimitiveBounds);

		return meshPrimitiveConcept; // Note: A Material component must be attached to the returned entity
	}


	void MeshPrimitiveComponent::AttachMeshPrimitiveComponent(
		fr::EntityManager& em,
		entt::entity owningEntity,
		gr::MeshPrimitive const* meshPrimitive,
		glm::vec3 const& positionMinXYZ,
		glm::vec3 const& positionMaxXYZ)
	{
		SEAssert(em.HasComponent<fr::TransformComponent>(owningEntity),
			"A MeshPrimitive's owningEntity requires a TransformComponent");
		SEAssert(em.HasComponent<fr::RenderDataComponent>(owningEntity),
			"A MeshPrimitive's owningEntity requires a RenderDataComponent");

		fr::RenderDataComponent* meshPrimRenderCmpt = 
			em.GetFirstInHierarchyAbove<fr::RenderDataComponent>(owningEntity);

		// Note: A Material component will typically need to be attached to the owningEntity
		AttachMeshPrimitiveComponentHelper(
			em, owningEntity, meshPrimitive, *meshPrimRenderCmpt, positionMinXYZ, positionMaxXYZ);
	}


	MeshPrimitiveComponent& MeshPrimitiveComponent::AttachRawMeshPrimitiveConcept(
		EntityManager& em,
		entt::entity owningEntity, 
		fr::RenderDataComponent const& sharedRenderDataCmpt, 
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
		entt::entity entity, MeshPrimitiveComponent const& meshPrimitiveComponent)
	{
		// Get the RenderDataID of the MeshConcept that owns the MeshPrimitive
		gr::RenderDataID owningMeshRenderDataID = gr::k_invalidRenderDataID;

		fr::EntityManager const* em = fr::EntityManager::Get();
		fr::Relationship const& meshPrimRelationship = em->GetComponent<fr::Relationship>(entity);
		entt::entity meshConceptEntity = meshPrimRelationship.GetParent();
		if (meshConceptEntity != entt::null) // null if the MeshPrimitive isn't owned by a MeshConcept
		{
			fr::RenderDataComponent const& meshConceptRenderComponent =
				em->GetComponent<fr::RenderDataComponent>(meshConceptEntity);

			owningMeshRenderDataID = meshConceptRenderComponent.GetRenderDataID();
			SEAssert(owningMeshRenderDataID != gr::k_invalidRenderDataID, "Invalid render data ID received from Mesh");
		}

		gr::MeshPrimitive::RenderData renderData{
			.m_meshPrimitiveParams = meshPrimitiveComponent.m_meshPrimitive->GetMeshParams(),
			.m_vertexStreams = {nullptr}, // Vertex streams copied below...
			.m_numVertexStreams = 0,
			.m_indexStream = meshPrimitiveComponent.m_meshPrimitive->GetIndexStream(),
			.m_hasMorphTargets = meshPrimitiveComponent.m_meshPrimitive->HasMorphTargets(),
			.m_interleavedMorphData = meshPrimitiveComponent.m_meshPrimitive->GetInterleavedMorphDataBuffer(),
			.m_morphTargetMetadata = meshPrimitiveComponent.m_meshPrimitive->GetMorphTargetMetadata(),
			.m_dataHash = meshPrimitiveComponent.m_meshPrimitive->GetDataHash(),
			.m_owningMeshRenderDataID = owningMeshRenderDataID,
		};

		std::vector<gr::MeshPrimitive::MeshVertexStream> const& vertexStreams =
			meshPrimitiveComponent.m_meshPrimitive->GetVertexStreams();
		uint8_t slotIdx = 0;
		for (gr::MeshPrimitive::MeshVertexStream const& stream : vertexStreams)
		{
			if (stream.m_vertexStream == nullptr) // We assume vertex streams are tightly packed
			{
				break;
			}

			renderData.m_vertexStreams[slotIdx++] = stream.m_vertexStream;
		}
		renderData.m_numVertexStreams = slotIdx;

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
			fr::RenderDataComponent::ShowImGuiWindow(em, meshPrimitive);

			fr::MeshPrimitiveComponent& meshPrimCmpt = em.GetComponent<fr::MeshPrimitiveComponent>(meshPrimitive);
			meshPrimCmpt.m_meshPrimitive->ShowImGuiWindow();

			// Material:
			fr::MaterialInstanceComponent* matCmpt = em.TryGetComponent<fr::MaterialInstanceComponent>(meshPrimitive);
			if (matCmpt)
			{
				fr::MaterialInstanceComponent::ShowImGuiWindow(em, meshPrimitive);
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

			// Skinning:
			fr::SkinningComponent::ShowImGuiWindow(em, meshPrimitive);

			ImGui::Unindent();
		}
	}
}