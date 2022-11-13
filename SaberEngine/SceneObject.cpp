#include "SceneObject.h"

using re::MeshPrimitive;
using gr::Transform;
using std::string;
using std::shared_ptr;
using std::make_shared;


namespace fr
{
	SceneObject::SceneObject(string const& name, Transform* parent)
		: en::NamedObject::NamedObject(name)
		, m_mesh(nullptr)
	{
		m_transform.SetParent(parent);
	}


	SceneObject::SceneObject(SceneObject const& rhs) : en::NamedObject::NamedObject(rhs.GetName())
	{
		m_transform = rhs.m_transform;
		m_mesh = rhs.m_mesh;
	}


	void SceneObject::AddMesh(std::shared_ptr<gr::Mesh> mesh)
	{
		SEAssert("Scene object already has a mesh", m_mesh == nullptr);

		SEAssert("Mesh transform is not this object. This isn't necessarily bad, just unexpected. TODO: Handle this if it occurs",
			mesh->GetTransform() == &m_transform);

		m_mesh = mesh;
	}
}

