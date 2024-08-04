// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IEventListener.h"
#include "Core/Interfaces/IEngineComponent.h"
#include "Core/Interfaces/INamedObject.h"


namespace fr
{
	class SceneManager final : public virtual en::IEngineComponent
	{
	public: // Helper for identifying the scene render system
		static constexpr char const* k_sceneRenderSystemName = "Scene";
		static const NameID k_sceneRenderSystemNameID;


	public:
		static SceneManager* Get(); // Singleton functionality


	public:
		SceneManager();
		SceneManager(SceneManager&&) = default;
		SceneManager& operator=(SceneManager&&) = default;
		~SceneManager() = default;

		// IEngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;


	public:
		void ShowImGuiWindow(bool*) const;


	private:
		bool Load(std::string const& relativeFilePath); // Filename and path, relative to the ..\Scenes\ dir


	private:
		NameID m_sceneRenderSystemNameID;

	private:
		SceneManager(SceneManager const&) = delete;
		void operator=(SceneManager const&) = delete;
	};
}

