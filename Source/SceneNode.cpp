// © 2022 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "NamedObject.h"
#include "SceneNode.h"
#include "Transform.h"


namespace fr
{
	gr::Transform* SceneNode::CreateSceneNodeEntity(char const* name, gr::Transform* parent)
	{
		fr::GameplayManager* gameplayMgr = fr::GameplayManager::Get();

		entt::entity sceneNodeEntity = gameplayMgr->CreateEntity();

		gameplayMgr->EmplaceComponent<en::NameComponent>(sceneNodeEntity, name);

		gr::Transform* sceneNodeTransform = 
			gameplayMgr->EmplaceComponent<gr::Transform>(sceneNodeEntity, parent);

		return sceneNodeTransform;
	}


	gr::Transform* SceneNode::CreateSceneNodeEntity(std::string const& name, gr::Transform* parent)
	{
		return CreateSceneNodeEntity(name.c_str(), parent);
	}
}

