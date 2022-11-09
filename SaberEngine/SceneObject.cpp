#include "SceneObject.h"

using re::MeshPrimitive;
using gr::Transform;
using std::string;
using std::shared_ptr;
using std::make_shared;


namespace fr
{
	SceneObject::SceneObject(string const& name, Transform* parent) :
		en::NamedObject::NamedObject(name)
	{
		m_transform.SetParent(parent);
	}


	SceneObject::SceneObject(SceneObject const& sceneObject) : en::NamedObject::NamedObject(sceneObject.GetName())
	{
		m_transform = sceneObject.m_transform;
	}


	void SceneObject::AddMeshPrimitive(shared_ptr<MeshPrimitive> meshPrimitive)
	{
		m_meshes.emplace_back(make_shared<gr::Mesh>(&m_transform, meshPrimitive));
	}
}

