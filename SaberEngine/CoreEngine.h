#pragma once

#include <memory>

#include "LogManager.h"
#include "TimeManager.h"
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
		void Update() override;

		// EventListener interface:
		void HandleEvent(en::EventManager::EventInfo const& eventInfo) override;

	
	private:
		bool ProcessCommandLineArgs(int argc, char** argv);


	private:	
		const double m_FixedTimeStep; // Regular step size, in ms

		bool m_isRunning;

		// Non-singleton engine components:
		std::shared_ptr<en::LogManager> const m_logManager;
		std::shared_ptr<en::TimeManager> const m_timeManager;


	private: 
		static CoreEngine* m_coreEngine;


	private:
		CoreEngine() = delete;
		CoreEngine(CoreEngine const&) = delete;
		CoreEngine(CoreEngine&&) = delete;
		CoreEngine& operator=(CoreEngine const&) = delete;
	};
}
