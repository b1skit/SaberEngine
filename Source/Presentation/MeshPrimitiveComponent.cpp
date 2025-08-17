// © 2023 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "MaterialInstanceComponent.h"
#include "MeshConcept.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "SkinningComponent.h"
#include "TransformComponent.h"

#include "Core/InvPtr.h"

#include "Renderer/MeshPrimitive.h"
#include "Renderer/VertexStream.h"


namespace
{
	void AttachMeshPrimitiveComponentHelper(
		pr::EntityManager& em,
		entt::entity owningEntity,
		core::InvPtr<gr::MeshPrimitive> const& meshPrimitive,
		pr::RenderDataComponent& meshPrimRenderCmpt,
		glm::vec3 const& positionMinXYZ,
		glm::vec3 const& positionMaxXYZ)
	{
		// MeshPrimitive:
		em.EmplaceComponent<pr::MeshPrimitiveComponent>(
			owningEntity,
			pr::MeshPrimitiveComponent{
				.m_meshPrimitive = meshPrimitive
			});

		pr::Relationship const& owningEntityRelationship = em.GetComponent<pr::Relationship>(owningEntity);

		const entt::entity encapsulatingBounds =
			owningEntityRelationship.GetFirstEntityInHierarchyAbove<pr::Mesh::MeshConceptMarker, pr::BoundsComponent>(em);

		// Bounds for the MeshPrimitive
		pr::BoundsComponent const& meshPrimitiveBounds = pr::BoundsComponent::AttachBoundsComponent(
			em,
			owningEntity,
			encapsulatingBounds,
			positionMinXYZ,
			positionMaxXYZ);	

		// Mark our new MeshPrimitive as dirty:
		em.EmplaceComponent<DirtyMarker<pr::MeshPrimitiveComponent>>(owningEntity);
	}
}


namespace pr
{
	entt::entity MeshPrimitiveComponent::CreateMeshPrimitiveConcept(
		pr::EntityManager& em,
		entt::entity owningEntity, 
		core::InvPtr<gr::MeshPrimitive> const& meshPrimitive,
		glm::vec3 const& positionMinXYZ,
		glm::vec3 const& positionMaxXYZ)
	{
		SEAssert(em.HasComponent<pr::RenderDataComponent>(owningEntity),
			"A MeshPrimitive's owningEntity requires a RenderDataComponent");

		entt::entity meshPrimitiveConcept = em.CreateEntity(meshPrimitive->GetName());

		// Relationship:
		pr::Relationship& meshPrimitiveRelationship = em.GetComponent<pr::Relationship>(meshPrimitiveConcept);
		meshPrimitiveRelationship.SetParent(em, owningEntity);

		// RenderDataComponent: A MeshPrimitive has its own RenderDataID, but shares the TransformID of its owningEntity
		// However, if the owning entity does not have a TransformComponent, we attach one to the meshPrimitiveConcept
		// entity instead (as it's possible the owning entity is associated with a shared TransformID, without having
		// the TransformComponent attached to it)
		pr::TransformComponent const* transformComponent = em.TryGetComponent<pr::TransformComponent>(owningEntity);
		if (!transformComponent)
		{
			transformComponent = &pr::TransformComponent::AttachTransformComponent(em, meshPrimitiveConcept);
		}

		pr::RenderDataComponent* meshPrimRenderCmpt = pr::RenderDataComponent::GetCreateRenderDataComponent(
			em, meshPrimitiveConcept, transformComponent->GetTransformID());

		meshPrimRenderCmpt->SetFeatureBit(gr::RenderObjectFeature::IsMeshPrimitiveConcept);

		AttachMeshPrimitiveComponentHelper(
			em, meshPrimitiveConcept, meshPrimitive, *meshPrimRenderCmpt, positionMinXYZ, positionMaxXYZ);

		// Set the mesh primitive bounds feature bit for the culling system
		meshPrimRenderCmpt->SetFeatureBit(gr::RenderObjectFeature::IsMeshPrimitiveBounds);

		return meshPrimitiveConcept; // Note: A Material component must be attached to the returned entity
	}


	void MeshPrimitiveComponent::AttachMeshPrimitiveComponent(
		pr::EntityManager& em,
		entt::entity owningEntity,
		core::InvPtr<gr::MeshPrimitive> const& meshPrimitive,
		glm::vec3 const& positionMinXYZ,
		glm::vec3 const& positionMaxXYZ)
	{
		SEAssert(em.HasComponent<pr::TransformComponent>(owningEntity),
			"A MeshPrimitive's owningEntity requires a TransformComponent");
		SEAssert(em.HasComponent<pr::RenderDataComponent>(owningEntity),
			"A MeshPrimitive's owningEntity requires a RenderDataComponent");

		pr::Relationship const& relationship = em.GetComponent<pr::Relationship>(owningEntity);
		
		pr::RenderDataComponent* meshPrimRenderCmpt = relationship.GetFirstInHierarchyAbove<pr::RenderDataComponent>(em);

		// Note: A Material component will typically need to be attached to the owningEntity
		AttachMeshPrimitiveComponentHelper(
			em, owningEntity, meshPrimitive, *meshPrimRenderCmpt, positionMinXYZ, positionMaxXYZ);
	}


	MeshPrimitiveComponent& MeshPrimitiveComponent::AttachRawMeshPrimitiveConcept(
		EntityManager& em,
		entt::entity owningEntity, 
		pr::RenderDataComponent const& sharedRenderDataCmpt, 
		core::InvPtr<gr::MeshPrimitive> const& meshPrimitive)
	{
		// MeshPrimitive:
		MeshPrimitiveComponent& meshPrimCmpt = *em.EmplaceComponent<MeshPrimitiveComponent>(
			owningEntity,
			MeshPrimitiveComponent
			{
				.m_meshPrimitive = meshPrimitive
			});

		// Mark our new MeshPrimitive as dirty:
		em.EmplaceComponent<DirtyMarker<pr::MeshPrimitiveComponent>>(owningEntity);

		return meshPrimCmpt;
	}


	gr::MeshPrimitive::RenderData MeshPrimitiveComponent::CreateRenderData(
		pr::EntityManager& em, entt::entity entity, MeshPrimitiveComponent const& meshPrimitiveComponent)
	{
		// Get the RenderDataID of the MeshConcept that owns the MeshPrimitive
		gr::RenderDataID owningMeshRenderDataID = gr::k_invalidRenderDataID;

		pr::Relationship const& meshPrimRelationship = em.GetComponent<pr::Relationship>(entity);
		entt::entity meshConceptEntity = meshPrimRelationship.GetParent();
		bool meshHasSkinning = false;
		if (meshConceptEntity != entt::null) // null if the MeshPrimitive isn't owned by a MeshConcept
		{
			pr::RenderDataComponent const& meshConceptRenderComponent =
				em.GetComponent<pr::RenderDataComponent>(meshConceptEntity);

			owningMeshRenderDataID = meshConceptRenderComponent.GetRenderDataID();
			SEAssert(owningMeshRenderDataID != gr::k_invalidRenderDataID, "Invalid render data ID received from Mesh");

			pr::SkinningComponent const* skinningCmpt = em.TryGetComponent<pr::SkinningComponent>(meshConceptEntity);
			if (skinningCmpt)
			{
				meshHasSkinning = true;
			}
		}

		gr::MeshPrimitive::RenderData renderData{
			.m_meshPrimitiveParams = meshPrimitiveComponent.m_meshPrimitive->GetMeshParams(),
			.m_vertexStreams = {nullptr}, // Vertex streams copied below...
			.m_numVertexStreams = 0,
			.m_indexStream = meshPrimitiveComponent.m_meshPrimitive->GetIndexStream(),
			.m_hasMorphTargets = meshPrimitiveComponent.m_meshPrimitive->HasMorphTargets(),
			.m_interleavedMorphData = meshPrimitiveComponent.m_meshPrimitive->GetInterleavedMorphDataBuffer(),
			.m_morphTargetMetadata = meshPrimitiveComponent.m_meshPrimitive->GetMorphTargetMetadata(),
			.m_meshHasSkinning = meshHasSkinning,
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


	void MeshPrimitiveComponent::ShowImGuiWindow(pr::EntityManager& em, entt::entity meshPrimitive)
	{
		pr::NameComponent const& nameCmpt = em.GetComponent<pr::NameComponent>(meshPrimitive);

		if (ImGui::CollapsingHeader(
			std::format("{}##{}", nameCmpt.GetName(), nameCmpt.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();
			
			// RenderDataComponent:
			pr::RenderDataComponent::ShowImGuiWindow(em, meshPrimitive);

			pr::MeshPrimitiveComponent& meshPrimCmpt = em.GetComponent<pr::MeshPrimitiveComponent>(meshPrimitive);
			meshPrimCmpt.m_meshPrimitive->ShowImGuiWindow();

			// Material:
			pr::MaterialInstanceComponent* matCmpt = em.TryGetComponent<pr::MaterialInstanceComponent>(meshPrimitive);
			if (matCmpt)
			{
				pr::MaterialInstanceComponent::ShowImGuiWindow(em, meshPrimitive);
			}
			else
			{
				ImGui::Indent();
				ImGui::TextColored(ImVec4(1, 0, 0, 1), "<no material>"); // e.g. deferred mesh
				ImGui::Unindent();
			}

			// Bounds
			pr::BoundsComponent::ShowImGuiWindow(em, meshPrimitive);

			// Transform:
			pr::Relationship const& relationship = em.GetComponent<pr::Relationship>(meshPrimitive);
			
			entt::entity transformOwner = entt::null;
			pr::TransformComponent* transformComponent =
				relationship.GetFirstAndEntityInHierarchyAbove<pr::TransformComponent>(transformOwner);
			
			ImGui::PushID(static_cast<uint64_t>(meshPrimitive));
			pr::TransformComponent::ShowImGuiWindow(em, transformOwner, static_cast<uint64_t>(meshPrimitive));
			ImGui::PopID();

			ImGui::Unindent();
		}
	}
}