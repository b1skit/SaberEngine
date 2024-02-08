// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "EngineComponent.h"
#include "EventListener.h"
#include "SceneData.h"


namespace fr
{
	class SceneManager final : public virtual en::EngineComponent
	{
	public:
		static SceneManager* Get(); // Singleton functionality
		static fr::SceneData* GetSceneData() { return SceneManager::Get()->m_sceneData.get(); }


	public:
		SceneManager();
		SceneManager(SceneManager&&) = default;
		SceneManager& operator=(SceneManager&&) = default;
		~SceneManager() = default;

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;


		void ShowSpawnImGuiWindow() const;


	private:
		std::shared_ptr<fr::SceneData> m_sceneData = nullptr;


	private:
		SceneManager(SceneManager const&) = delete;
		void operator=(SceneManager const&) = delete;
	};
}

