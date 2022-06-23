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
			this->transform = sceneObject.transform;
		}

		// SaberObject interface:
		/*void Update() { SaberObject::Update(); }*/

		// Getters/Setters:
		inline Transform* GetTransform() { return &transform; }


	protected:
		Transform transform;
		

	private:
		


	};
}
