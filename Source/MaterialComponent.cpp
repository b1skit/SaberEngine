// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "MaterialComponent.h"
#include "MeshPrimitiveComponent.h"
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


	MaterialComponent& MaterialComponent::AttachMaterialComponent(
		fr::EntityManager& em,
		entt::entity meshPrimitiveConcept,
		std::shared_ptr<gr::Material> sceneMaterial)
	{
		SEAssert(sceneMaterial != nullptr, "Cannot attach a null material");
		SEAssert(em.HasComponent<fr::MeshPrimitiveComponent>(meshPrimitiveConcept),
			"Material components must be attached to entities with a MeshPrimitiveComponent");
		SEAssert(em.HasComponent<gr::RenderDataComponent>(meshPrimitiveConcept),
			"Material components must be attached to entities with a RenderDataComponent");

		// Attach the material component:		
		fr::MaterialComponent* matComponent =
			em.EmplaceComponent<fr::MaterialComponent>(meshPrimitiveConcept, sceneMaterial.get());

		// Mark our Material as dirty:
		em.EmplaceOrReplaceComponent<DirtyMarker<fr::MaterialComponent>>(meshPrimitiveConcept);

		return *matComponent;
	}


	void MaterialComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity)
	{
		fr::MaterialComponent const& matCmpt = em.GetComponent<fr::MaterialComponent>(owningEntity);

		if (ImGui::CollapsingHeader(std::format("Material \"{}\"##{}", 
			matCmpt.m_material->GetName(), matCmpt.m_material->GetUniqueID()).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			gr::RenderDataComponent::ShowImGuiWindow(em, owningEntity);

			// Material:
			fr::MaterialComponent& matComponent = em.GetComponent<fr::MaterialComponent>(owningEntity);

			// ECS_CONVERSION: TODO COMPLETE THIS FUNCTIONALITY

			ImGui::Unindent();
		}
	}
}