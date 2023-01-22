// © 2022 Adam Badke. All rights reserved.
#pragma once

#include "LogManager.h"
#include "EngineComponent.h"
#include "EventListener.h"
#include "ThreadPool.h"
#include "Window.h"


namespace en
{
	class CoreEngine final : public virtual en::EngineComponent, public virtual en::EventListener
	{
	public: // Singleton functionality:	
		static inline CoreEngine* Get() { return m_coreEngine; }
		static inline en::ThreadPool* GetThreadPool() { return &m_coreEngine->m_threadPool; }

	public:
		CoreEngine();
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

		en::Window* GetWindow() const { return m_window.get(); }


	private:
		// How much time we want to spend updating the game state (in ms) before we render a new frame
		const double m_fixedTimeStep;

		bool m_isRunning;

		uint64_t m_frameNum;

		en::ThreadPool m_threadPool;

		std::unique_ptr<en::Window> m_window;

		std::unique_ptr<std::barrier<>> m_copyBarrier;
		

	private: 
		static CoreEngine* m_coreEngine;


	private:
		CoreEngine(CoreEngine const&) = delete;
		CoreEngine(CoreEngine&&) = delete;
		CoreEngine& operator=(CoreEngine const&) = delete;
	};
}
