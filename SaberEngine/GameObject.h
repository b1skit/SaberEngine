#pragma once

#include "SceneObject.h"
#include "RenderMesh.h"


namespace fr
{
	class GameObject : public virtual fr::SceneObject
	{
	public:
		// TODO: GameObject ctors should all take a parent Transform*
		explicit GameObject(std::string const& name);
		GameObject(GameObject const& gameObject);

		GameObject(GameObject&&) = default;
		~GameObject() = default;

		GameObject() = delete;		

		// SaberObject interface:
		void Update() override {}

		// EventListener interface:
		void HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo) override {}

		// Getters/Setters:
		void AddMeshPrimitive(std::shared_ptr<gr::Mesh> meshPrimitive);
		inline std::vector<std::shared_ptr<gr::RenderMesh>> const& GetRenderMeshes() const { return m_renderMeshes; }


	private:
		std::vector<std::shared_ptr<gr::RenderMesh>> m_renderMeshes;
	};
}


