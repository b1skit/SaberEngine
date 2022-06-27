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
		// No-arg constructor (Don't use this!):
		GameObject() : SceneObject::SceneObject("Unnamed GameObject")
		{
			m_renderable.SetTransform(&m_transform);
		}

		// String constructor:
		GameObject(string name) : SceneObject::SceneObject(name) 
		{
			m_renderable.SetTransform(&m_transform);
		}

		// String and renderable constructor:
		GameObject(string name, Renderable renderable);

		// Copy constructor:
		GameObject(const GameObject& gameObject)
		{
			m_renderable = gameObject.m_renderable;
			m_transform = gameObject.m_transform;

			m_renderable.SetTransform(&m_transform);
		}

		// SaberObject interface:
		void Update() override { }

		// EventListener interface:
		void HandleEvent(EventInfo const* eventInfo) {}

		// Getters/Setters:
		inline Renderable* GetRenderable() { return &m_renderable; }
		

	protected:
		Renderable m_renderable;


	private:
		

		
	};
}


