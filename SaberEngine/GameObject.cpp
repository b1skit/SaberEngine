#include "GameObject.h"

namespace SaberEngine
{
	GameObject::GameObject(string name, Renderable renderable) : SceneObject::SceneObject(name)
	{
		this->renderable = renderable;
		this->renderable.SetTransform(&this->transform);
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

