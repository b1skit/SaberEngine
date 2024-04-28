// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "IEventListener.h"
#include "SceneData.h"

#include "Core\Interfaces\IEngineComponent.h"


namespace fr
{
	class SceneManager final : public virtual en::IEngineComponent
	{
	public: // Helper for identifying the scene render system
		static constexpr char const* k_sceneRenderSystemName = "Scene";
		static const NameID k_sceneRenderSystemNameID;


	public:
		static SceneManager* Get(); // Singleton functionality
		static fr::SceneData* GetSceneData() { return SceneManager::Get()->m_sceneData.get(); }


	public:
		SceneManager();
		SceneManager(SceneManager&&) = default;
		SceneManager& operator=(SceneManager&&) = default;
		~SceneManager() = default;

		// IEngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;


		void ShowImGuiWindow(bool*) const;


	private:
		std::shared_ptr<fr::SceneData> m_sceneData = nullptr;
		NameID m_sceneRenderSystemNameID;

	private:
		SceneManager(SceneManager const&) = delete;
		void operator=(SceneManager const&) = delete;
	};
}

