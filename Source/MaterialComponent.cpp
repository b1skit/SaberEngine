// © 2023 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "MarkerComponents.h"
#include "MaterialComponent.h"
#include "MeshPrimitiveComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"


namespace fr
{
	gr::Material::RenderData Material::CreateRenderData(MaterialComponent const& matComponent)
	{
		return gr::Material::RenderData{
			.m_material = matComponent.m_material
		};
	}


	Material::MaterialComponent& Material::AttachMaterialConcept(
		entt::entity meshPrimitiveConcept,
		std::shared_ptr<gr::Material> sceneMaterial)
	{
		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		SEAssert("Cannot attach a null material", sceneMaterial != nullptr);
		SEAssert("Attempting to attach a Material component without a MeshPrimitiveComponent. This (currently) doesn't "
			"make sense",
			fr::Relationship::HasComponentInParentHierarchy<fr::MeshPrimitive::MeshPrimitiveComponent>(meshPrimitiveConcept));

		entt::entity materialEntity = gpm.CreateEntity(sceneMaterial->GetName());

		// Attach the material component:		
		fr::Material::MaterialComponent* matComponent =
			gpm.EmplaceComponent<fr::Material::MaterialComponent>(materialEntity, sceneMaterial.get());

		// Relate the material to the owning mesh primitive:
		fr::Relationship& materialRelationship = fr::Relationship::AttachRelationshipComponent(gpm, materialEntity);
		materialRelationship.SetParent(gpm, meshPrimitiveConcept);

		gr::RenderDataComponent const& meshPrimRenderData =
			gpm.GetComponent<gr::RenderDataComponent>(meshPrimitiveConcept);

		gr::RenderDataComponent::AttachSharedRenderDataComponent(gpm, materialEntity, meshPrimRenderData);

		// Mark our Material as dirty:
		gpm.EmplaceOrReplaceComponent<DirtyMarker<fr::Material::MaterialComponent>>(materialEntity);

		return *matComponent;
	}
}