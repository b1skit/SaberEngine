// © 2022 Adam Badke. All rights reserved.
#include "SceneNode.h"

using re::MeshPrimitive;
using gr::Transform;
using std::string;
using std::shared_ptr;
using std::make_shared;


namespace fr
{
	SceneNode::SceneNode(Transform* parent)
		: Transformable(parent)
	{
	}
}

