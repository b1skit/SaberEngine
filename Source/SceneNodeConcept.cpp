// © 2022 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "NameComponent.h"
#include "SceneNodeConcept.h"
#include "Transform.h"


namespace fr
{
	gr::Transform* SceneNode::Create(char const* name, gr::Transform* parent)
	{
		fr::GameplayManager* gameplayMgr = fr::GameplayManager::Get();

		entt::entity sceneNodeEntity = gameplayMgr->CreateEntity(name);

		gr::Transform* sceneNodeTransform = 
			gameplayMgr->EmplaceComponent<gr::Transform>(sceneNodeEntity, parent);

		return sceneNodeTransform;
	}


	gr::Transform* SceneNode::Create(std::string const& name, gr::Transform* parent)
	{
		return Create(name.c_str(), parent);
	}
}

