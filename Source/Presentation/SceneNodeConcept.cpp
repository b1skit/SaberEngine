// � 2022 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "RelationshipComponent.h"
#include "SceneNodeConcept.h"


namespace fr
{
	entt::entity SceneNode::Create(EntityManager& em, std::string_view name, entt::entity parent)
	{
		entt::entity sceneNodeEntity = em.CreateEntity(name);

		fr::Relationship& sceneNodeRelationship = em.GetComponent<fr::Relationship>(sceneNodeEntity);
		sceneNodeRelationship.SetParent(em, parent);
		
		return sceneNodeEntity;
	}
}

