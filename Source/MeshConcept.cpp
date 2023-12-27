// © 2022 Adam Badke. All rights reserved.
#include "BoundsComponent.h"
#include "EntityManager.h"
#include "ImGuiUtils.h"
#include "MeshConcept.h"
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


	void Mesh::ShowImGuiWindow()
	{
		// ECS_CONVERSON: TODO: Restore this functionality

		//fr::EntityManager& em = *fr::EntityManager::Get();

		//if (ImGui::CollapsingHeader(
		//	std::format("{}##{}", GetName(), util::PtrToID(this)).c_str(), ImGuiTreeNodeFlags_None))
		//{
		//	ImGui::Indent();
		//	const std::string uniqueIDStr = std::to_string(util::PtrToID(this));

		//	if (ImGui::CollapsingHeader(
		//		std::format("Transform:##{}", util::PtrToID(this)).c_str(), ImGuiTreeNodeFlags_None))
		//	{
		//		ImGui::Indent();
		//		m_ownerTransform->ShowImGuiWindow();
		//		ImGui::Unindent();
		//	}

		//	if (ImGui::CollapsingHeader(
		//		std::format("Mesh Bounds:##{}", util::PtrToID(this)).c_str(), ImGuiTreeNodeFlags_None))
		//	{
		//		ImGui::Indent();
		//		m_localBounds.ShowImGuiWindow();
		//		ImGui::Unindent();
		//	}

		//	if (ImGui::CollapsingHeader(
		//		std::format("Mesh Primitives ({}):##{}", m_meshPrimitives.size(), util::PtrToID(this)).c_str(), 
		//		ImGuiTreeNodeFlags_None))
		//	{
		//		ImGui::Indent();
		//		for (size_t i = 0; i < m_meshPrimitives.size(); i++)
		//		{
		//			m_meshPrimitives[i]->ShowImGuiWindow();
		//		}
		//		ImGui::Unindent();
		//	}
		//	ImGui::Unindent();
		//}
	}
}