#pragma once

#include "SceneObject.h"	// Base class
#include "Renderable.h"


namespace SaberEngine
{
	class GameObject : public virtual SceneObject
	{
	public:
		GameObject(std::string const& name) : SceneObject::SceneObject(name),
			m_renderable( std::make_shared<Renderable>() ) {m_renderable->SetTransform(&m_transform); }

		GameObject(std::string const& name, std::shared_ptr<Renderable> const& renderable);

		GameObject(GameObject const& gameObject) : SceneObject(gameObject.GetName())
		{
			m_renderable = gameObject.m_renderable;
			m_transform = gameObject.m_transform;
			m_renderable->SetTransform(&m_transform);
		}

		GameObject(GameObject&&) = default;
		~GameObject() = default;

		GameObject() = delete;		

		// SaberObject interface:
		void Update() override {}

		// EventListener interface:
		void HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo) override {}

		// Getters/Setters:
		inline std::shared_ptr<Renderable> GetRenderable() { return m_renderable; }


	private:
		std::shared_ptr<Renderable> m_renderable;
	};
}


