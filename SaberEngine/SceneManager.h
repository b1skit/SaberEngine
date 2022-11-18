#pragma once

#include "EngineComponent.h"
#include "EventListener.h"
#include "SceneData.h"


namespace en
{
	class SceneManager : public virtual en::EngineComponent
	{
	public:
		static SceneManager* Get(); // Singleton functionality
		static fr::SceneData* GetSceneData() { return SceneManager::Get()->m_sceneData.get(); }


	public:
		SceneManager();
		~SceneManager() = default;

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(const double stepTimeMs) override;

	private:
		std::shared_ptr<fr::SceneData> m_sceneData = nullptr;


	private:
		SceneManager(SceneManager const&) = delete;
		SceneManager(SceneManager&&) = delete;
		void operator=(SceneManager const&) = delete;
	};
}

