#include "GameObject.h"

using std::string;


namespace fr
{
	GameObject::GameObject(string const& name) : 
		fr::SceneObject::SceneObject(name),
		m_renderable(std::make_shared<SaberEngine::Renderable>()) 
	{
		m_renderable->SetTransform(&m_transform);
	}


	GameObject::GameObject(string const& name, std::shared_ptr<SaberEngine::Renderable> const& renderable) :
		SceneObject::SceneObject(name),
			m_renderable(renderable)
	{
		m_renderable->SetTransform(&m_transform);
	}


	GameObject::GameObject(GameObject const& gameObject) : 
		fr::SceneObject(gameObject.GetName()),
		m_renderable(gameObject.m_renderable)
	{
		m_transform = gameObject.m_transform;
		m_renderable->SetTransform(&m_transform);
	}

}

