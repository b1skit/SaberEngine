// � 2022 Adam Badke. All rights reserved.
#pragma once


namespace fr
{
	class EntityManager;


	// A scene node is the foundational concept of anything representable in a scene: It has a Name, and Relationship
	class SceneNode
	{
	public:
		static entt::entity Create(EntityManager&, std::string_view name, entt::entity parent);
	};
}