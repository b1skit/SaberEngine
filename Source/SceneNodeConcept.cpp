// © 2022 Adam Badke. All rights reserved.
#include "GameplayManager.h"
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
		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		entt::entity sceneNodeEntity = gpm.CreateEntity(name);

		fr::Transform* parentTransform = nullptr;
		if (parent != entt::null)
		{
			SEAssert("Parent entity must have a TransformComponent", 
				gpm.HasComponent<fr::TransformComponent>(parent));

			parentTransform = &gpm.GetComponent<fr::TransformComponent>(parent).GetTransform();
		}
		fr::TransformComponent const& transformComponent = 
			fr::TransformComponent::AttachTransformComponent(gpm, sceneNodeEntity, parentTransform);

		// Mark the transform as dirty:
		gpm.EmplaceComponent<DirtyMarker<fr::TransformComponent>>(sceneNodeEntity);
		
		gr::RenderDataComponent::AttachNewRenderDataComponent(gpm, sceneNodeEntity, transformComponent.GetTransformID());

		fr::Relationship& sceneNodeRelationship = fr::Relationship::AttachRelationshipComponent(gpm, sceneNodeEntity);
		sceneNodeRelationship.SetParent(gpm, parent);

		return sceneNodeEntity;
	}


	entt::entity SceneNode::Create(std::string const& name, entt::entity parent)
	{
		return Create(name.c_str(), parent);
	}


	fr::Transform& SceneNode::GetSceneNodeTransform(entt::entity entity)
	{
		fr::GameplayManager& gpm = *fr::GameplayManager::Get();

		SEAssert("Entity does not have a TransformComponent", gpm.HasComponent<fr::TransformComponent>(entity));

		return gpm.GetComponent<fr::TransformComponent>(entity).GetTransform();
	}
}

