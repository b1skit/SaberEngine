#include "GameObject.h"

namespace SaberEngine
{
	GameObject::GameObject(string name, Renderable renderable) : SceneObject::SceneObject(name)
	{
		m_renderable = renderable;
		m_renderable.SetTransform(&m_transform);
	}

	//GameObject::~GameObject()
	//{
	//}

	//void SaberEngine::GameObject::Update()
	//{

	//}

	//void SaberEngine::GameObject::HandleEvent(EventInfo const * eventInfo)
	//{

	//}
}

