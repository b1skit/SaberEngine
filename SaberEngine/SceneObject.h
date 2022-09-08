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
		SceneObject(std::string const& newName) : SaberObject::SaberObject(newName) {}

		SceneObject() = delete;

		SceneObject(SceneObject&&) = default;
		SceneObject(const SceneObject& sceneObject) : SaberObject(sceneObject.GetName())
		{
			m_transform = sceneObject.m_transform;
		}
		virtual ~SceneObject() = 0;

		// SaberObject interface:
		void Update() override = 0;

		// Getters/Setters:
		inline gr::Transform* GetTransform() { return &m_transform; }
		inline gr::Transform const* GetTransform() const { return &m_transform; }


	protected:
		gr::Transform m_transform;
		
	private:
		
	};


	// We need to provide a destructor implementation since it's pure virtual
	inline SceneObject::~SceneObject() {}
}
