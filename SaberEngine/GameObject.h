#pragma once

#include "SceneObject.h"
#include "Renderable.h"


namespace fr
{
	class GameObject : public virtual fr::SceneObject
	{
	public:
		GameObject(std::string const& name);
		GameObject(std::string const& name, std::shared_ptr<SaberEngine::Renderable> const& renderable);
		GameObject(GameObject const& gameObject);

		GameObject(GameObject&&) = default;
		~GameObject() = default;

		GameObject() = delete;		

		// SaberObject interface:
		void Update() override {}

		// EventListener interface:
		void HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo) override {}

		// Getters/Setters:
		inline std::shared_ptr<SaberEngine::Renderable> GetRenderable() { return m_renderable; }


	private:
		std::shared_ptr<SaberEngine::Renderable> m_renderable;
	};
}


