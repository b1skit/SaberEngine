// Scene object interface

#pragma once

#include "SaberObject.h"	// Base class
#include "EventListener.h"	// Base class
#include "Transform.h"


namespace SaberEngine
{
	class SceneObject : public SaberObject, public virtual EventListener
	{
	public:
		SceneObject() : SaberObject::SaberObject("Unnamed SceneObject") {}
		SceneObject(string newName) : SaberObject::SaberObject(newName) {}

		SceneObject(SceneObject&&) = default;
		SceneObject(const SceneObject& sceneObject) : SaberObject(sceneObject.GetName())
		{
			m_transform = sceneObject.m_transform;
		}
		virtual ~SceneObject() = 0;

		// SaberObject interface:
		void Update() override = 0;

		// Getters/Setters:
		inline Transform* GetTransform() { return &m_transform; }


	protected:
		Transform m_transform;
		
	private:
		
	};


	// We need to provide a destructor implementation since it's pure virutal
	inline SceneObject::~SceneObject() {}
}
