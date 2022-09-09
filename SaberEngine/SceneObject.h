#pragma once

#include "SaberObject.h"
#include "EventListener.h"
#include "Transform.h"


namespace fr
{
	class SceneObject : public en::SaberObject, public virtual en::EventListener
	{
	public:
		SceneObject(std::string const& newName) : en::SaberObject::SaberObject(newName) {}

		SceneObject(const SceneObject& sceneObject) : en::SaberObject(sceneObject.GetName())
		{
			m_transform = sceneObject.m_transform;
		}

		SceneObject(SceneObject&&) = default;
		SceneObject& operator=(SceneObject const&) = default;

		SceneObject() = delete;

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
