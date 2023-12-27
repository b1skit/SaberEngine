// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace gr
{
	class Transform;
}

namespace fr
{
	// A scene node is the foundational concept of anything representable in a scene: It has a Transform, and a Name
	class SceneNode
	{
	public:
		static entt::entity Create(char const* name, entt::entity parent);
		static entt::entity Create(std::string const& name, entt::entity parent);


	public:
		static fr::Transform& GetTransform(entt::entity); // ECS_CONVERSION: THIS USES THE GPM, PASS IT AS AN ARG
	};
}