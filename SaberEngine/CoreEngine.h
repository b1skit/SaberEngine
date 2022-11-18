#pragma once

#include <memory>

#include "LogManager.h"
#include "EngineComponent.h"


namespace en
{
	class CoreEngine : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public: // Singleton functionality:	
		static inline CoreEngine* Get() { return m_coreEngine; }

	public:
		explicit CoreEngine(int argc, char** argv);
		~CoreEngine() = default;

		// Lifetime flow:
		void Startup();
		void Run();
		void Stop();
		void Shutdown();

		// NamedObject interface:
		void Update(const double stepTimeMs) override;

		// EventListener interface:
		void HandleEvents() override;

	
	private:
		bool ProcessCommandLineArgs(int argc, char** argv);


	private:
		// How much time we want to spend updating the game state (in ms) before we render a new frame
		const double m_fixedTimeStep;

		bool m_isRunning;

		// Non-singleton engine components:
		std::shared_ptr<en::LogManager> const m_logManager;


	private: 
		static CoreEngine* m_coreEngine;


	private:
		CoreEngine() = delete;
		CoreEngine(CoreEngine const&) = delete;
		CoreEngine(CoreEngine&&) = delete;
		CoreEngine& operator=(CoreEngine const&) = delete;
	};
}
