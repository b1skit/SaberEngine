// © 2022 Adam Badke. All rights reserved.
#include "SceneNode.h"



namespace fr
{
	SceneNode::SceneNode(std::string const& name, gr::Transform* parent)
		: NamedObject(name + "_SceneNode")
		, Transformable(name, parent)
	{
	}


	SceneNode::SceneNode(SceneNode const& sceneObject)
		: NamedObject(sceneObject.GetName())
		, Transformable(sceneObject.GetName(), sceneObject.GetTransform()->GetParent())
	{

	}
}

