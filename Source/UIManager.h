// © 2023 Adam Badke. All rights reserved.
#pragma once
#include "EngineComponent.h"
#include "EventListener.h"


namespace fr
{
	class UIManager : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public:
		static UIManager* Get(); // Singleton functionality


	public:
		UIManager();
		~UIManager() = default;


	public: // EngineComponent interface:
		void Startup() override;
		void Shutdown() override;
		void Update(uint64_t frameNum, double stepTimeMs) override;

		void HandleEvents() override;

	private:
		void UpdateImGui();


	private:
		bool m_imguiMenuVisible;
	};
}