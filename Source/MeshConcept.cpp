// © 2022 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "ImGuiUtils.h"
#include "MeshConcept.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "TransformComponent.h"


namespace fr
{
	entt::entity Mesh::AttachMeshConcept(entt::entity sceneNode, char const* name)
	{
		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		SEAssert("A Mesh concept requires a Transform via a SceneNode. The sceneNode should have this already",
			gpm.HasComponent<fr::TransformComponent>(sceneNode));
		SEAssert("A mesh requires a Relationship with a SceneNode. The sceneNode parent should have this already",
			gpm.HasComponent<fr::Relationship>(sceneNode));

		entt::entity meshEntity = gpm.CreateEntity(name);

		gpm.EmplaceComponent<fr::Mesh::MeshConceptMarker>(meshEntity);

		fr::TransformComponent const& transformComponent = gpm.GetComponent<fr::TransformComponent>(sceneNode);

		gr::RenderDataComponent::AttachNewRenderDataComponent(gpm, meshEntity, transformComponent.GetTransformID());

		fr::BoundsComponent::AttachBoundsComponent(gpm, meshEntity); // Mesh bounds: Encompasses all attached primitive bounds

		fr::Relationship& meshRelationship = fr::Relationship::AttachRelationshipComponent(gpm, meshEntity);
		meshRelationship.SetParent(gpm, sceneNode);

		return meshEntity;
	}


	void Mesh::ShowImGuiWindow()
	{
		// ECS_CONVERSON: TODO: Restore this functionality

		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

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