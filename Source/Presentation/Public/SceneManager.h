// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/Inventory.h"

#include "Core/Interfaces/IEventListener.h"
#include "Core/Interfaces/IEngineComponent.h"
#include "Core/Interfaces/INamedObject.h"


namespace fr
{
	class SceneManager final : public virtual en::IEngineComponent, public virtual core::IEventListener
	{
	public:
		static SceneManager* Get(); // Singleton functionality


	public:
		SceneManager();
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
		void SetInventory(core::Inventory*); // Dependency injection: Call once immediately after creation
		core::Inventory* GetInventory() const;
	private:
		core::Inventory* m_inventory;


	public:
		void ShowImGuiWindow(bool*) const;


	private:
		void Reset();

		void CreateDefaultSceneResources();

		void ImportFile(std::string const& filePath); // Filename and path, relative to the ..\Scenes\ dir


	private:
		SceneManager(SceneManager const&) = delete;
		void operator=(SceneManager const&) = delete;
	};


	inline void SceneManager::SetInventory(core::Inventory* inventory)
	{
		m_inventory = inventory;
	}


	inline core::Inventory* SceneManager::GetInventory() const
	{
		return m_inventory;
	}
}

