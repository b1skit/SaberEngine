#include "GameObject.h"

using std::string;


namespace SaberEngine
{
	GameObject::GameObject(string const& name, std::shared_ptr<Renderable> const& renderable) : 
		SceneObject::SceneObject(name),
			m_renderable{ renderable }
	{
		m_renderable->SetTransform(&m_transform);
	}

	//void SaberEngine::GameObject::Update()
	//{

	//}

	//void SaberEngine::GameObject::HandleEvent(std::shared_ptr<EventInfo const> eventInfo)
	//{

	//}
}

