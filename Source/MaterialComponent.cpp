// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "MaterialComponent.h"
#include "MeshPrimitiveComponent.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"


namespace fr
{
	gr::Material::RenderData MaterialComponent::CreateRenderData(
		MaterialComponent const& matComponent, fr::NameComponent const&)
	{
		return gr::Material::RenderData{
			.m_material = matComponent.m_material
		};
	}


	MaterialComponent& MaterialComponent::AttachMaterialConcept(
		fr::EntityManager& em,
		entt::entity meshPrimitiveConcept,
		std::shared_ptr<gr::Material> sceneMaterial)
	{
		SEAssert("Cannot attach a null material", sceneMaterial != nullptr);
		SEAssert("Attempting to attach a Material component without a MeshPrimitiveComponent. This (currently) doesn't "
			"make sense",
			em.IsInHierarchyAbove<fr::MeshPrimitiveComponent>(meshPrimitiveConcept));

		entt::entity materialEntity = em.CreateEntity(sceneMaterial->GetName());

		// Attach the material component:		
		fr::MaterialComponent* matComponent =
			em.EmplaceComponent<fr::MaterialComponent>(materialEntity, sceneMaterial.get());

		// Relate the material to the owning mesh primitive:
		fr::Relationship& materialRelationship = em.GetComponent<fr::Relationship>(materialEntity);
		materialRelationship.SetParent(em, meshPrimitiveConcept);

		gr::RenderDataComponent const& meshPrimRenderData =
			em.GetComponent<gr::RenderDataComponent>(meshPrimitiveConcept);

		gr::RenderDataComponent::AttachSharedRenderDataComponent(em, materialEntity, meshPrimRenderData);

		// Mark our Material as dirty:
		em.EmplaceOrReplaceComponent<DirtyMarker<fr::MaterialComponent>>(materialEntity);

		return *matComponent;
	}


	void MaterialComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity)
	{
		fr::NameComponent const& nameComponent = em.GetComponent<fr::NameComponent>(owningEntity);
		fr::MaterialComponent& matComponent = em.GetComponent<fr::MaterialComponent>(owningEntity);

		if (ImGui::CollapsingHeader(
			std::format("{}##{}", nameComponent.GetName(), nameComponent.GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// ECS_CONVERSION: TODO COMPLETE THIS FUNCTIONALITY

			ImGui::Unindent();
		}
	}
}