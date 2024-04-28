// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "IEventListener.h"
#include "Core\LogManager.h"
#include "Window.h"

#include "Core\Interfaces\IEngineComponent.h"


namespace en
{
	class EngineApp final : public virtual en::IEngineComponent, public virtual en::IEventListener
	{
	public: // Singleton functionality:	
		static inline EngineApp* Get() { return m_engineApp; }


	public:
		EngineApp();
		EngineApp(EngineApp&&) = default;
		EngineApp& operator=(EngineApp&&) = default;
		~EngineApp() = default;

		// Lifetime flow:
		void Startup();
		void Run();
		void Stop();
		void Shutdown();

		// INamedObject interface:
		void Update(uint64_t frameNum, double stepTimeMs) override;

		// IEventListener interface:
		void HandleEvents() override;

		en::Window* GetWindow() const { return m_window.get(); }


	private:
		// How much time we want to spend updating the game state (in ms) before we render a new frame
		const double m_fixedTimeStep;

		bool m_isRunning;

		uint64_t m_frameNum;

		std::unique_ptr<en::Window> m_window;

		std::unique_ptr<std::barrier<>> m_copyBarrier;
		

	private: 
		static EngineApp* m_engineApp;


	private:
		EngineApp(EngineApp const&) = delete;
		EngineApp& operator=(EngineApp const&) = delete;
	};
}
