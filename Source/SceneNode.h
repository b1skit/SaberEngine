// © 2022 Adam Badke. All rights reserved.
#pragma once


namespace gr
{
	class Transform;
}

namespace fr
{
	class SceneNode
	{
	public:
		static gr::Transform* CreateSceneNodeEntity(char const* name, gr::Transform* parent);
		static gr::Transform* CreateSceneNodeEntity(std::string const& name, gr::Transform* parent);
	};
}


