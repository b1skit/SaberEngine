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
	entt::entity SceneNode::Create(EntityManager& em, char const* name, entt::entity parent)
	{
		entt::entity sceneNodeEntity = em.CreateEntity(name);

		fr::Relationship& sceneNodeRelationship = em.GetComponent<fr::Relationship>(sceneNodeEntity);
		sceneNodeRelationship.SetParent(em, parent);

		fr::Transform* parentTransform = nullptr;
		if (parent != entt::null)
		{
			SEAssert("Parent entity must have a TransformComponent", em.HasComponent<fr::TransformComponent>(parent));

			parentTransform = &em.GetComponent<fr::TransformComponent>(parent).GetTransform();
		}
		
		fr::TransformComponent::AttachTransformComponent(em, sceneNodeEntity, parentTransform);
		
		return sceneNodeEntity;
	}


	entt::entity SceneNode::Create(EntityManager& em, std::string const& name, entt::entity parent)
	{
		return Create(em, name.c_str(), parent);
	}


	fr::Transform& SceneNode::GetTransform(fr::EntityManager& em, entt::entity entity)
	{
		SEAssert("Entity does not have a TransformComponent", em.HasComponent<fr::TransformComponent>(entity));

		return em.GetComponent<fr::TransformComponent>(entity).GetTransform();
	}
}

