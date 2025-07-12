// © 2022 Adam Badke. All rights reserved.
#include "EntityManager.h"
#include "RelationshipComponent.h"
#include "SceneNodeConcept.h"


namespace pr
{
	entt::entity SceneNode::Create(EntityManager& em, std::string_view name, entt::entity parent)
	{
		entt::entity sceneNodeEntity = em.CreateEntity(name);

		pr::Relationship& sceneNodeRelationship = em.GetComponent<pr::Relationship>(sceneNodeEntity);
		sceneNodeRelationship.SetParent(em, parent);
		
		return sceneNodeEntity;
	}
}

