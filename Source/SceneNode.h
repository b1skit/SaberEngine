// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "Transformable.h"
#include "Mesh.h"
#include "Updateable.h"



namespace fr
{
	// A concrete implementation of Transformable; Used to construct a scene object transformation hierarchy
	class SceneNode final : public virtual fr::Transformable
	{
	public:
		explicit SceneNode(std::string const& name, gr::Transform* parent);

		SceneNode(SceneNode const& sceneObject);
		SceneNode(SceneNode&&) = default;
		SceneNode& operator=(SceneNode const&) = default;
		SceneNode& operator=(SceneNode&&) = default;
		~SceneNode() = default;


	private:
		SceneNode() = delete;
	};
}


