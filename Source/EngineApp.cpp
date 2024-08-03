// © 2022 Adam Badke. All rights reserved.
#include "EngineApp.h"
#include "EntityManager.h"
#include "SceneManager.h"
#include "UIManager.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/EventManager.h"
#include "Core/InputManager.h"
#include "Core/LogManager.h"
#include "Core/PerformanceTimer.h"
#include "Core/ProfilingMarkers.h"
#include "Core/ThreadPool.h"

#include "Renderer/Context.h"
#include "Renderer/RenderManager.h"
#include "Renderer/Window.h"


namespace
{
	constexpr size_t k_numSystemThreads = 2;
}

namespace app
{
	EngineApp*	EngineApp::m_engineApp = nullptr;


	EngineApp::EngineApp()
		: m_fixedTimeStep(1000.0 / 120.0) // 1000/120 = 8.33ms per update
		, m_isRunning(false)
		, m_frameNum(0)
	{
		m_engineApp = this;
		m_copyBarrier = std::make_unique<std::barrier<>>(k_numSystemThreads);
		core::ThreadPool::NameCurrentThread(L"Main Thread");
	}


	void EngineApp::Startup()
	{
		SEBeginCPUEvent("app::EngineApp::Startup");

		LOG("EngineApp starting...");

		core::ThreadPool::Get()->Startup();

		// Start the logging thread:
		core::LogManager::Get()->Startup(
			core::Config::Get()->KeyExists(core::configkeys::k_showSystemConsoleWindowCmdLineArg));

		// Render thread:
		re::RenderManager* renderManager = re::RenderManager::Get();
		core::ThreadPool::Get()->EnqueueJob([&]()
			{
				core::ThreadPool::NameCurrentThread(L"Render Thread");
				renderManager->Lifetime(m_copyBarrier.get()); 
			});
		renderManager->ThreadStartup(); // Initializes context

		// Now the Context has been created, we can get the window
		app::Window* window = re::Context::Get()->GetWindow();
		
		// Don't capture the mouse while we're loading
		window->SetRelativeMouseMode(false);

		// Start managers:
		core::EventManager* eventManager = core::EventManager::Get();
		eventManager->Startup();
		eventManager->Subscribe(core::EventManager::EngineQuit, this);

		en::InputManager::Get()->Startup(); // Now that the window is created

		fr::SceneManager::Get()->Startup(); // Load assets

		// Create entity/component representations now that the scene data is loaded
		fr::EntityManager::Get()->Startup();

		renderManager->ThreadInitialize(); // Create render systems, close buffer registration

		fr::UIManager::Get()->Startup();

		m_isRunning = true;

		// We're done loading: Capture the mouse
		window->SetRelativeMouseMode(true);

		SEEndCPUEvent();
	}


	// Main game loop
	void EngineApp::Run()
	{
		LOG("\nEngineApp: Starting main game loop\n");

		core::EventManager* eventManager = core::EventManager::Get();
		core::LogManager* logManager = core::LogManager::Get();
		en::InputManager* inputManager = en::InputManager::Get();
		fr::EntityManager* entityManager = fr::EntityManager::Get();
		fr::SceneManager* sceneManager = fr::SceneManager::Get();
		re::RenderManager* renderManager = re::RenderManager::Get();
		fr::UIManager* uiManager = fr::UIManager::Get();

		// Process any events that might have occurred during startup:
		eventManager->Update(m_frameNum, 0.0);

		// Initialize game loop timing:
		double elapsed = (double)m_fixedTimeStep; // Ensure we pump Updates once before the 1st render

		util::PerformanceTimer outerLoopTimer;
		util::PerformanceTimer innerLoopTimer;
		double lastOuterFrameTime = 0.0;

		while (m_isRunning)
		{
			SEBeginCPUEvent("app::EngineApp::Run frame outer loop");

			outerLoopTimer.Start();

			SEBeginCPUEvent("app::EngineApp::Update");
			EngineApp::Update(m_frameNum, lastOuterFrameTime);
			SEEndCPUEvent();

			// Update components until enough time has passed to trigger a render.
			// Or, continue rendering frames until it's time to update again
			elapsed += lastOuterFrameTime;
			while (elapsed >= m_fixedTimeStep)
			{	
				SEBeginCPUEvent("app::EngineApp::Run frame inner loop");

				elapsed -= m_fixedTimeStep;

				// Pump our events/input:
				SEBeginCPUEvent("core::EventManager::Update");
				eventManager->Update(m_frameNum, lastOuterFrameTime);
				SEEndCPUEvent();

				SEBeginCPUEvent("en::InputManager::Update");
				inputManager->Update(m_frameNum, lastOuterFrameTime);
				SEEndCPUEvent();

				SEBeginCPUEvent("en::EntityManager::Update");
				entityManager->Update(m_frameNum, m_fixedTimeStep);
				SEEndCPUEvent();

				SEBeginCPUEvent("fr::SceneManager::Update");
				sceneManager->Update(m_frameNum, m_fixedTimeStep); // Updates all of the scene objects
				SEEndCPUEvent();

				// AI, physics, etc should also be pumped here (eventually)

				SEEndCPUEvent();
			}

			SEBeginCPUEvent("fr::UIManager::Update");
			uiManager->Update(m_frameNum, m_fixedTimeStep);
			SEEndCPUEvent();

			SEBeginCPUEvent("fr::EntityManager::EnqueueRenderUpdates");
			entityManager->EnqueueRenderUpdates();
			SEEndCPUEvent();

			// Pump the render thread, and wait for it to signal copying is complete:
			SEBeginCPUEvent("app::EngineApp::Run Wait on copy step");
			renderManager->EnqueueUpdate({m_frameNum, lastOuterFrameTime});
			m_copyBarrier->arrive_and_wait();
			SEEndCPUEvent();

			++m_frameNum;

			lastOuterFrameTime = outerLoopTimer.StopMs();

			SEEndCPUEvent();
		}
	}


	void EngineApp::Stop()
	{
		m_isRunning = false;
	}


	void EngineApp::Shutdown()
	{
		SEBeginCPUEvent("app::EngineApp::Shutdown");

		LOG("EngineApp shutting down...");

		fr::UIManager::Get()->Shutdown();
		
		fr::EntityManager::Get()->Shutdown();

		fr::SceneManager::Get()->Shutdown();

		// We need to signal the render thread to shut down and wait on it to complete before we can start destroying
		// anything it might be using
		re::RenderManager::Get()->ThreadShutdown();

		en::InputManager::Get()->Shutdown();
		core::EventManager::Get()->Shutdown();

		core::LogManager::Get()->Shutdown(); // Destroy last

		core::ThreadPool::Get()->Stop();

		SEEndCPUEvent();
	}

	
	void EngineApp::Update(uint64_t frameNum, double stepTimeMs)
	{
		SEBeginCPUEvent("app::EngineApp::Update");

		HandleEvents();

		SEEndCPUEvent();
	}


	void EngineApp::HandleEvents()
	{
		SEBeginCPUEvent("app::EngineApp::HandleEvents");

		while (HasEvents())
		{
			core::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_type)
			{
			case core::EventManager::EngineQuit:
			{
				Stop();
			}
			break;
			default:
				break;
			}
		}

		SEEndCPUEvent();
	}
}