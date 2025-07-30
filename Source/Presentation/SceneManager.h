// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/Interfaces/IEventListener.h"
#include "Core/Interfaces/IEngineComponent.h"


namespace pr
{
	class EntityManager;


	class SceneManager final : public virtual en::IEngineComponent, public virtual core::IEventListener
	{
	public:
		SceneManager(pr::EntityManager*);

		SceneManager(SceneManager&&) noexcept = default;
		SceneManager& operator=(SceneManager&&) noexcept = default;
		~SceneManager() = default;

		// IEngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;

		// IEventListener interface:
		void HandleEvents() override;


	public:
		void ShowImGuiWindow(bool*) const;


	private:
		void Reset();

		void CreateDefaultSceneResources();

		void ImportFile(std::string const& filePath); // Filename and path, relative to the ..\Scenes\ dir

	private:
		pr::EntityManager* m_entityManager;


	private:
		SceneManager(SceneManager const&) = delete;
		void operator=(SceneManager const&) = delete;
	};
}

