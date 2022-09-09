#include "GameObject.h"

using std::string;


namespace fr
{
	GameObject::GameObject(string const& name) : 
		fr::SceneObject::SceneObject(name),
		m_renderMesh(std::make_shared<gr::RenderMesh>(&m_transform))
	{
	}


	GameObject::GameObject(string const& name, std::shared_ptr<gr::RenderMesh> const& rendermesh) :
		SceneObject::SceneObject(name),
			m_renderMesh(rendermesh)
	{
		m_renderMesh->SetTransform(&m_transform);
	}


	GameObject::GameObject(GameObject const& gameObject) : 
		fr::SceneObject(gameObject.GetName()),
		m_renderMesh(gameObject.m_renderMesh)
	{
		m_transform = gameObject.m_transform;
	}

}

