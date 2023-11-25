// © 2022 Adam Badke. All rights reserved.
#include "GameplayManager.h"
#include "NameComponent.h"
#include "SceneNodeEntity.h"
#include "Transform.h"


namespace fr
{
	gr::Transform* SceneNodeEntity::CreateSceneNodeEntity(char const* name, gr::Transform* parent)
	{
		fr::GameplayManager* gameplayMgr = fr::GameplayManager::Get();

		entt::entity sceneNodeEntity = gameplayMgr->CreateEntity();

		gameplayMgr->EmplaceComponent<fr::NameComponent>(sceneNodeEntity, name);

		gr::Transform* sceneNodeTransform = 
			gameplayMgr->EmplaceComponent<gr::Transform>(sceneNodeEntity, parent);

		return sceneNodeTransform;
	}


	gr::Transform* SceneNodeEntity::CreateSceneNodeEntity(std::string const& name, gr::Transform* parent)
	{
		return CreateSceneNodeEntity(name.c_str(), parent);
	}
}

