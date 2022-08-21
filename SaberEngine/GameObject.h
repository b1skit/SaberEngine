// Game object interface
// Game object interface.
// All in-scene/game objects should inherit from this interface

#pragma once

#include "SceneObject.h"	// Base class
#include "Renderable.h"


namespace SaberEngine
{
	class GameObject : public SceneObject
	{
	public:
		GameObject() = delete;

		// String constructor:
		GameObject(string name) : SceneObject::SceneObject(name)
		{
			m_renderable = std::make_shared<Renderable>();
			m_renderable->SetTransform(&m_transform);
		}

		// String and renderable constructor:
		GameObject(string name, std::shared_ptr<Renderable> const& renderable);

		// Copy constructor:
		GameObject(const GameObject& gameObject)
		{
			m_renderable = gameObject.m_renderable;
			m_transform = gameObject.m_transform;

			m_renderable->SetTransform(&m_transform);
		}

		// SaberObject interface:
		void Update() override { }

		// EventListener interface:
		void HandleEvent(EventInfo const* eventInfo) {}

		// Getters/Setters:
		inline std::shared_ptr<Renderable> GetRenderable() { return m_renderable; }

		

	protected:
		std::shared_ptr<Renderable> m_renderable;


	private:
		

		
	};
}


