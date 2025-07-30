// © 2023 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "MaterialInstanceComponent.h"
#include "MeshPrimitiveComponent.h"
#include "RenderDataComponent.h"

#include "Core/Inventory.h"
#include "Core/InvPtr.h"

#include "Core/Util/ImGuiUtils.h"


namespace pr
{
	gr::Material::MaterialInstanceRenderData MaterialInstanceComponent::CreateRenderData(
		pr::EntityManager& em, entt::entity, MaterialInstanceComponent const& matComponent)
	{
		SEAssert(matComponent.m_instanceData.m_textures.size() == matComponent.m_instanceData.m_samplers.size(),
			"Texture/sampler array size mismatch. We assume the all material instance arrays are the same size");

		return matComponent.m_instanceData;
	}


	MaterialInstanceComponent& MaterialInstanceComponent::AttachMaterialComponent(
		pr::EntityManager& em,
		entt::entity meshPrimitiveConcept,
		core::InvPtr<gr::Material> const& sceneMaterial)
	{
		SEAssert(sceneMaterial != nullptr, "Cannot attach a null material");
		SEAssert(em.HasComponent<pr::MeshPrimitiveComponent>(meshPrimitiveConcept),
			"Material components must be attached to entities with a MeshPrimitiveComponent");
		SEAssert(em.HasComponent<pr::RenderDataComponent>(meshPrimitiveConcept),
			"Material components must be attached to entities with a RenderDataComponent");

		// Attach the material component:
		pr::MaterialInstanceComponent& matComponent = 
			*em.EmplaceComponent<pr::MaterialInstanceComponent>(meshPrimitiveConcept, PrivateCTORTag{}, sceneMaterial);

		// Mark our Material as dirty:
		em.EmplaceOrReplaceComponent<DirtyMarker<pr::MaterialInstanceComponent>>(meshPrimitiveConcept);

		return matComponent;
	}


	MaterialInstanceComponent::MaterialInstanceComponent(PrivateCTORTag, core::InvPtr<gr::Material> const& srcMat)
		: m_srcMaterial(srcMat)
		, m_isDirty(true)
	{
		// Copy data from the source material to make a material instance:
		m_srcMaterial->InitializeMaterialInstanceData(m_instanceData);
	}


	void MaterialInstanceComponent::ShowImGuiWindow(pr::EntityManager& em, entt::entity owningEntity)
	{
		pr::MaterialInstanceComponent const& matCmpt = em.GetComponent<pr::MaterialInstanceComponent>(owningEntity);

		const uint64_t ptrToID = util::PtrToID(&matCmpt);

		if (ImGui::CollapsingHeader(std::format("Material instance \"{}\"##{}", 
			matCmpt.m_instanceData.m_materialName, ptrToID).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			pr::RenderDataComponent::ShowImGuiWindow(em, owningEntity);

			// MaterialInstanceRenderData:
			pr::MaterialInstanceComponent& matComponent = em.GetComponent<pr::MaterialInstanceComponent>(owningEntity);
			matComponent.m_isDirty |= gr::Material::ShowImGuiWindow(matComponent.m_instanceData);

			if (ImGui::Button(std::format("Reset##{}", ptrToID).c_str()))
			{
				core::InvPtr<gr::Material> const& srcMaterial =
					core::Inventory::Get<gr::Material>(matComponent.m_instanceData.m_materialName);

				srcMaterial->InitializeMaterialInstanceData(matComponent.m_instanceData);
				matComponent.m_isDirty = true;
			}

			ImGui::Unindent();
		}
	}
}