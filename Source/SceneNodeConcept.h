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
		static gr::Transform* Create(char const* name, gr::Transform* parent);
		static gr::Transform* Create(std::string const& name, gr::Transform* parent);
	};
}