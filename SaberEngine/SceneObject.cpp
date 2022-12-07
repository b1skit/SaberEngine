#include "SceneObject.h"

using re::MeshPrimitive;
using gr::Transform;
using std::string;
using std::shared_ptr;
using std::make_shared;


namespace fr
{
	SceneObject::SceneObject(Transform* parent)
	{
		m_transform.SetParent(parent);
	}
}

