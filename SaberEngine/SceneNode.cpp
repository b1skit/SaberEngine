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

