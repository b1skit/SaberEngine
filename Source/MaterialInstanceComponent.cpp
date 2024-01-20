// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "MaterialInstanceComponent.h"
#include "MeshPrimitiveComponent.h"
#include "RenderDataComponent.h"
#include "SceneManager.h"


namespace fr
{
	gr::Material::MaterialInstanceData MaterialInstanceComponent::CreateRenderData(
		MaterialInstanceComponent const& matComponent, fr::NameComponent const&)
	{
		return matComponent.m_instanceData;
	}


	MaterialInstanceComponent& MaterialInstanceComponent::AttachMaterialComponent(
		fr::EntityManager& em,
		entt::entity meshPrimitiveConcept,
		std::shared_ptr<gr::Material const> sceneMaterial)
	{
		SEAssert(sceneMaterial != nullptr, "Cannot attach a null material");
		SEAssert(em.HasComponent<fr::MeshPrimitiveComponent>(meshPrimitiveConcept),
			"Material components must be attached to entities with a MeshPrimitiveComponent");
		SEAssert(em.HasComponent<gr::RenderDataComponent>(meshPrimitiveConcept),
			"Material components must be attached to entities with a RenderDataComponent");

		// Attach the material component:
		em.EmplaceComponent<fr::MaterialInstanceComponent>(meshPrimitiveConcept);
		fr::MaterialInstanceComponent& matComponent = 
			em.GetComponent<fr::MaterialInstanceComponent>(meshPrimitiveConcept);

		// Copy data from the source material to make a material instance:
		sceneMaterial->PackMaterialInstanceData(matComponent.m_instanceData);

		// Mark our Material as dirty:
		em.EmplaceOrReplaceComponent<DirtyMarker<fr::MaterialInstanceComponent>>(meshPrimitiveConcept);

		return matComponent;
	}


	void MaterialInstanceComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity)
	{
		fr::MaterialInstanceComponent const& matCmpt = em.GetComponent<fr::MaterialInstanceComponent>(owningEntity);

		if (ImGui::CollapsingHeader(std::format("Material \"{}\"##{}", 
			matCmpt.m_instanceData.m_materialName, matCmpt.m_instanceData.m_uniqueID).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			gr::RenderDataComponent::ShowImGuiWindow(em, owningEntity);

			// Material:
			fr::MaterialInstanceComponent& matComponent = em.GetComponent<fr::MaterialInstanceComponent>(owningEntity);
			matComponent.m_isDirty = gr::Material::ShowImGuiWindow(matComponent.m_instanceData);

			if (ImGui::Button("Reset"))
			{
				gr::Material const* srcMateral = 
					fr::SceneManager::GetSceneData()->GetMaterial(matComponent.m_instanceData.m_materialName).get();

				srcMateral->PackMaterialInstanceData(matComponent.m_instanceData);
				matComponent.m_isDirty = true;
			}

			ImGui::Unindent();
		}
	}
}