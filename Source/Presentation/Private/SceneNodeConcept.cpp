// © 2022 Adam Badke. All rights reserved.
#include "Private/EntityManager.h"
#include "Private/RelationshipComponent.h"
#include "Private/SceneNodeConcept.h"


namespace fr
{
	entt::entity SceneNode::Create(EntityManager& em, char const* name, entt::entity parent)
	{
		entt::entity sceneNodeEntity = em.CreateEntity(name);

		fr::Relationship& sceneNodeRelationship = em.GetComponent<fr::Relationship>(sceneNodeEntity);
		sceneNodeRelationship.SetParent(em, parent);
		
		return sceneNodeEntity;
	}


	entt::entity SceneNode::Create(EntityManager& em, std::string const& name, entt::entity parent)
	{
		return Create(em, name.c_str(), parent);
	}
}

