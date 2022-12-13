#pragma once

#include <memory>

#include "LogManager.h"
#include "EngineComponent.h"
#include "ThreadPool.h"


namespace en
{
	class CoreEngine : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public: // Singleton functionality:	
		static inline CoreEngine* Get() { return m_coreEngine; }
		static inline ThreadPool* GetThreadPool() { return &m_coreEngine->m_threadPool; }

	public:
		explicit CoreEngine(int argc, char** argv);
		~CoreEngine() = default;

		// Lifetime flow:
		void Startup();
		void Run();
		void Stop();
		void Shutdown();

		// NamedObject interface:
		void Update(uint64_t frameNum, double stepTimeMs) override;

		// EventListener interface:
		void HandleEvents() override;

	
	private:
		bool ProcessCommandLineArgs(int argc, char** argv);


	private:
		// How much time we want to spend updating the game state (in ms) before we render a new frame
		const double m_fixedTimeStep;

		bool m_isRunning;

		uint64_t m_frameNum;

		en::ThreadPool m_threadPool;

	private: 
		static CoreEngine* m_coreEngine;


	private:
		CoreEngine() = delete;
		CoreEngine(CoreEngine const&) = delete;
		CoreEngine(CoreEngine&&) = delete;
		CoreEngine& operator=(CoreEngine const&) = delete;
	};
}
