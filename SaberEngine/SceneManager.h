#pragma once

#include "EngineComponent.h"
#include "EventListener.h"
#include "SceneData.h"


namespace en
{
	class SceneManager : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public: // Singleton functionality:		
		static SceneManager* Get();
	private:
		static std::unique_ptr<SceneManager> m_instance;

	public:
		SceneManager();
		~SceneManager() = default;

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update() override;

		// EventListener interface:
		void HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo) override;

		// Member functions:
		inline std::shared_ptr<fr::SceneData> GetSceneData() { return m_sceneData; }
		inline std::shared_ptr<fr::SceneData const> const GetSceneData() const { return m_sceneData; }

	private:
		std::shared_ptr<fr::SceneData> m_sceneData = nullptr;


	


	private:
		SceneManager(SceneManager const&) = delete;
		SceneManager(SceneManager&&) = delete;
		void operator=(SceneManager const&) = delete;
	};
}

