#pragma once

#include "EngineComponent.h"
#include "EventListener.h"
#include "SceneData.h"


namespace en
{
	class SceneManager : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public:
		SceneManager();
		~SceneManager() = default;

		SceneManager(SceneManager const&) = delete;
		SceneManager(SceneManager&&) = delete;
		void operator=(SceneManager const&) = delete;

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		// EventListener interface:
		void HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo) override;

		// Member functions:
		inline std::shared_ptr<fr::SceneData> GetScene() { return m_currentScene; }
		inline std::shared_ptr<fr::SceneData const> const GetScene() const { return m_currentScene; }

	private:
		std::shared_ptr<fr::SceneData> m_currentScene = nullptr;
	};
}

