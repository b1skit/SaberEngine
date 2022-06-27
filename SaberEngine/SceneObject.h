// Scene object interface

#pragma once

#include "SaberObject.h"	// Base class
#include "EventListener.h"	// Base class
#include "Transform.h"


namespace SaberEngine
{
	class SceneObject : public SaberObject, public EventListener
	{
	public:
		SceneObject() : SaberObject::SaberObject("Unnamed SceneObject") { }
		SceneObject(string newName) : SaberObject::SaberObject(newName) {}

		// Copy constructor:
		SceneObject(const SceneObject& sceneObject) : SaberObject(sceneObject.GetName())
		{
			m_transform = sceneObject.m_transform;
		}

		// SaberObject interface:
		/*void Update() { SaberObject::Update(); }*/

		// Getters/Setters:
		inline Transform* GetTransform() { return &m_transform; }


	protected:
		Transform m_transform;
		

	private:
		


	};
}
