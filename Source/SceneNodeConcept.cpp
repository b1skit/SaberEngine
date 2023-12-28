// © 2022 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "MarkerComponents.h"
#include "NameComponent.h"
#include "RelationshipComponent.h"
#include "RenderDataComponent.h"
#include "SceneNodeConcept.h"
#include "TransformComponent.h"


namespace fr
{
	entt::entity SceneNode::Create(char const* name, entt::entity parent)
	{
		fr::EntityManager& em = *fr::EntityManager::Get();

		entt::entity sceneNodeEntity = em.CreateEntity(name);

		fr::Transform* parentTransform = nullptr;
		if (parent != entt::null)
		{
			SEAssert("Parent entity must have a TransformComponent", 
				em.HasComponent<fr::TransformComponent>(parent));

			parentTransform = &em.GetComponent<fr::TransformComponent>(parent).GetTransform();
		}
		fr::TransformComponent const& transformComponent = 
			fr::TransformComponent::AttachTransformComponent(em, sceneNodeEntity, parentTransform);

		// Mark the transform as dirty:
		em.EmplaceComponent<DirtyMarker<fr::TransformComponent>>(sceneNodeEntity);
		
		gr::RenderDataComponent::AttachNewRenderDataComponent(em, sceneNodeEntity, transformComponent.GetTransformID());

		fr::Relationship& sceneNodeRelationship = em.GetComponent<fr::Relationship>(sceneNodeEntity);
		sceneNodeRelationship.SetParent(em, parent);

		return sceneNodeEntity;
	}


	entt::entity SceneNode::Create(std::string const& name, entt::entity parent)
	{
		return Create(name.c_str(), parent);
	}


	fr::Transform& SceneNode::GetTransform(fr::EntityManager& em, entt::entity entity)
	{
		SEAssert("Entity does not have a TransformComponent", em.HasComponent<fr::TransformComponent>(entity));

		return em.GetComponent<fr::TransformComponent>(entity).GetTransform();
	}
}

