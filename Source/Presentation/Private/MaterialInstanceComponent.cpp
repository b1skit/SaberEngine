// © 2023 Adam Badke. All rights reserved.
#include "Private/EntityManager.h"
#include "Private/MarkerComponents.h"
#include "Private/MaterialInstanceComponent.h"
#include "Private/MeshPrimitiveComponent.h"
#include "Private/RenderDataComponent.h"

#include "Core/Inventory.h"
#include "Core/InvPtr.h"

#include "Core/Util/ImGuiUtils.h"


namespace fr
{
	gr::Material::MaterialInstanceRenderData MaterialInstanceComponent::CreateRenderData(
		entt::entity, MaterialInstanceComponent const& matComponent)
	{
		return matComponent.m_instanceData;
	}


	MaterialInstanceComponent& MaterialInstanceComponent::AttachMaterialComponent(
		fr::EntityManager& em,
		entt::entity meshPrimitiveConcept,
		core::InvPtr<gr::Material> const& sceneMaterial)
	{
		SEAssert(sceneMaterial != nullptr, "Cannot attach a null material");
		SEAssert(em.HasComponent<fr::MeshPrimitiveComponent>(meshPrimitiveConcept),
			"Material components must be attached to entities with a MeshPrimitiveComponent");
		SEAssert(em.HasComponent<fr::RenderDataComponent>(meshPrimitiveConcept),
			"Material components must be attached to entities with a RenderDataComponent");

		// Attach the material component:
		fr::MaterialInstanceComponent& matComponent = 
			*em.EmplaceComponent<fr::MaterialInstanceComponent>(meshPrimitiveConcept, PrivateCTORTag{}, sceneMaterial);

		// Mark our Material as dirty:
		em.EmplaceOrReplaceComponent<DirtyMarker<fr::MaterialInstanceComponent>>(meshPrimitiveConcept);

		return matComponent;
	}


	MaterialInstanceComponent::MaterialInstanceComponent(PrivateCTORTag, core::InvPtr<gr::Material> const& srcMat)
		: m_srcMaterial(srcMat)
		, m_isDirty(true)
	{
		// Copy data from the source material to make a material instance:
		m_srcMaterial->InitializeMaterialInstanceData(m_instanceData);
	}


	void MaterialInstanceComponent::ShowImGuiWindow(fr::EntityManager& em, entt::entity owningEntity)
	{
		fr::MaterialInstanceComponent const& matCmpt = em.GetComponent<fr::MaterialInstanceComponent>(owningEntity);

		const uint64_t ptrToID = util::PtrToID(&matCmpt);

		if (ImGui::CollapsingHeader(std::format("Material instance \"{}\"##{}", 
			matCmpt.m_instanceData.m_materialName, ptrToID).c_str(), ImGuiTreeNodeFlags_None))
		{
			ImGui::Indent();

			// RenderDataComponent:
			fr::RenderDataComponent::ShowImGuiWindow(em, owningEntity);

			// MaterialInstanceRenderData:
			fr::MaterialInstanceComponent& matComponent = em.GetComponent<fr::MaterialInstanceComponent>(owningEntity);
			matComponent.m_isDirty |= gr::Material::ShowImGuiWindow(matComponent.m_instanceData);

			if (ImGui::Button(std::format("Reset##{}", ptrToID).c_str()))
			{
				core::InvPtr<gr::Material> const& srcMaterial =
					em.GetInventory()->Get<gr::Material>(matComponent.m_instanceData.m_materialName);

				srcMaterial->InitializeMaterialInstanceData(matComponent.m_instanceData);
				matComponent.m_isDirty = true;
			}

			ImGui::Unindent();
		}
	}
}