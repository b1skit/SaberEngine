// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "EngineComponent.h"
#include "EventListener.h"
#include "SceneData.h"
#include "Batch.h"


namespace en
{
	class SceneManager final : public virtual en::EngineComponent
	{
	public:
		struct InstancedMeshParams // TODO: Is there a better place for this?
		{
			static constexpr char const* const s_shaderName = "InstancedMeshParams"; // Not counted towards size of struct
		};


	public:
		static SceneManager* Get(); // Singleton functionality
		static fr::SceneData* GetSceneData() { return SceneManager::Get()->m_sceneData.get(); }


	public:
		SceneManager();
		~SceneManager() = default;

		// EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;

		std::vector<re::Batch>& GetSceneBatches();

		void FinalUpdate(); // Called once after final Update call in main game loop

	private:
		void BuildSceneBatches();
		std::vector<re::Batch> m_sceneBatches;

		std::shared_ptr<fr::SceneData> m_sceneData = nullptr;


	private:
		SceneManager(SceneManager const&) = delete;
		SceneManager(SceneManager&&) = delete;
		void operator=(SceneManager const&) = delete;
	};
}

