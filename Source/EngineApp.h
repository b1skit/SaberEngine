// © 2022 Adam Badke. All rights reserved.
#pragma once
#include "Core/LogManager.h"

#include "Core/Interfaces/IEngineComponent.h"
#include "Core/Interfaces/IEventListener.h"


namespace app
{
	class EngineApp final : public virtual en::IEngineComponent, public virtual core::IEventListener
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


	private:
		// How much time we want to spend updating the game state (in ms) before we render a new frame
		static constexpr double k_fixedTimeStep = 1000.0 / 120.0; // 1000/120 = 8.33ms per update;

		bool m_isRunning;

		uint64_t m_frameNum;

		std::unique_ptr<std::barrier<>> m_copyBarrier;
		

	private: 
		static EngineApp* m_engineApp;


	private:
		EngineApp(EngineApp const&) = delete;
		EngineApp& operator=(EngineApp const&) = delete;
	};
}
