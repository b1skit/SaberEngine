#pragma once

#include "EngineComponent.h"
#include "EventListener.h"
#include "SceneData.h"


namespace en
{
	class SceneManager : public virtual en::EngineComponent
	{
	public: // Singleton functionality:		
		static SceneManager* Get();
		static fr::SceneData* GetSceneData() { return m_instance->m_sceneData.get(); }
	private:
		static std::unique_ptr<SceneManager> m_instance;

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

