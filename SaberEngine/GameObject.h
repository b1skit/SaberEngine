#pragma once

#include "SceneObject.h"
#include "RenderMesh.h"


namespace fr
{
	class GameObject : public virtual fr::SceneObject
	{
	public:
		GameObject(std::string const& name);
		GameObject(std::string const& name, std::shared_ptr<gr::RenderMesh> const& rendermesh);
		GameObject(GameObject const& gameObject);

		GameObject(GameObject&&) = default;
		~GameObject() = default;

		GameObject() = delete;		

		// SaberObject interface:
		void Update() override {}

		// EventListener interface:
		void HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo) override {}

		// Getters/Setters:
		inline std::shared_ptr<gr::RenderMesh> GetRenderMesh() { return m_renderMesh; }


	private:
		std::shared_ptr<gr::RenderMesh> m_renderMesh;
	};
}


