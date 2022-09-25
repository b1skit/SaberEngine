#include "GameObject.h"

using gr::Mesh;
using std::string;
using std::shared_ptr;
using std::make_shared;


namespace fr
{
	GameObject::GameObject(string const& name) : 
		fr::SceneObject::SceneObject(name)
	{
	}


	GameObject::GameObject(GameObject const& gameObject) : fr::SceneObject(gameObject.GetName())
	{
		m_transform = gameObject.m_transform;
	}


	void GameObject::AddMeshPrimitive(shared_ptr<Mesh> meshPrimitive)
	{
		m_renderMeshes.emplace_back(make_shared<gr::RenderMesh>(&m_transform, meshPrimitive));
	}
}

