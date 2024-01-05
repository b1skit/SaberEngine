// � 2022 Adam Badke. All rights reserved.
#pragma once


namespace fr
{
	class EntityManager;
	class Transform;


	// A scene node is the foundational concept of anything representable in a scene: It has a Transform, and a Name
	class SceneNode
	{
	public:
		static entt::entity Create(EntityManager&, char const* name, entt::entity parent);
		static entt::entity Create(EntityManager&, std::string const& name, entt::entity parent);


	public:
		static fr::Transform& GetTransform(fr::EntityManager&, entt::entity);
	};
}