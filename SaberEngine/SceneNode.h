#pragma once

#include "Transformable.h"
#include "Mesh.h"
#include "NamedObject.h"
#include "Updateable.h"



namespace fr
{
	// A concrete implementation of Transformable; Used to construct a scene object transformation hierarchy
	class SceneNode : public virtual fr::Transformable
	{
	public:
		explicit SceneNode(gr::Transform* parent);

		SceneNode(SceneNode const& sceneObject) = default;
		SceneNode(SceneNode&&) = default;
		SceneNode& operator=(SceneNode const&) = default;
		SceneNode& operator=(SceneNode&&) = default;
		~SceneNode() = default;


	private:
		SceneNode() = delete;
	};
}


