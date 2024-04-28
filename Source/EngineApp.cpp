// © 2022 Adam Badke. All rights reserved.
#include "Config.h"
#include "EngineApp.h"
#include "EntityManager.h"
#include "EventManager.h"
#include "InputManager.h"
#include "Platform.h"
#include "ProfilingMarkers.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "Core\ThreadPool.h"
#include "UIManager.h"

#include "Core\Assert.h"
#include "Core\LogManager.h"
#include "Core\PerformanceTimer.h"


namespace
{
	constexpr size_t k_numSystemThreads = 2;
}

namespace en
{
	EngineApp*	EngineApp::m_engineApp = nullptr;


	EngineApp::EngineApp()
		: m_fixedTimeStep(1000.0 / 120.0) // 1000/120 = 8.33ms per update
		, m_isRunning(false)
		, m_frameNum(0)
		, m_window(nullptr)
	{
		m_engineApp = this;
		m_copyBarrier = std::make_unique<std::barrier<>>(k_numSystemThreads);
		core::ThreadPool::NameCurrentThread(L"Main Thread");
	}


	void EngineApp::Startup()
	{
		SEBeginCPUEvent("en::EngineApp::Startup");

		LOG("EngineApp starting...");

		core::ThreadPool::Get()->Startup();

		// Start the logging thread:
		core::LogManager::Get()->Startup(
			en::Config::Get()->KeyExists(core::configkeys::k_showSystemConsoleWindowCmdLineArg));

		// Create a window:
		std::string commandLineArgs;
		en::Config::Get()->TryGetValue<std::string>(core::configkeys::k_commandLineArgsValueKey, commandLineArgs);

		std::string const& windowTitle = std::format("{} {}", 
			en::Config::Get()->GetValue<std::string>("windowTitle"), 
			commandLineArgs);
		const int xRes = en::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		const int yRes = en::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);

		m_window = std::make_unique<en::Window>(); // Ensure Window exists for first callbacks triggered by Create
		const bool windowCreated = m_window->Create(windowTitle, xRes, yRes);
		SEAssert(windowCreated, "Failed to create a window");

		// Don't capture the mouse while we're loading
		m_window->SetRelativeMouseMode(false);

		// Render thread:
		re::RenderManager* renderManager = re::RenderManager::Get();
		core::ThreadPool::Get()->EnqueueJob([&]()
			{
				core::ThreadPool::NameCurrentThread(L"Render Thread");
				renderManager->Lifetime(m_copyBarrier.get()); 
			});
		renderManager->ThreadStartup(); // Initializes context

		// Start managers:
		en::EventManager* eventManager = en::EventManager::Get();
		eventManager->Startup();
		eventManager->Subscribe(en::EventManager::EngineQuit, this);

		en::InputManager::Get()->Startup(); // Now that the window is created

		fr::SceneManager::Get()->Startup(); // Load assets

		// Create entity/component representations now that the scene data is loaded
		fr::EntityManager::Get()->Startup();

		renderManager->ThreadInitialize(); // Create render systems, close buffer registration

		fr::UIManager::Get()->Startup();

		m_isRunning = true;

		// We're done loading: Capture the mouse
		m_window->SetRelativeMouseMode(true);

		SEEndCPUEvent();
	}


	// Main game loop
	void EngineApp::Run()
	{
		LOG("\nEngineApp: Starting main game loop\n");

		en::EventManager* eventManager = en::EventManager::Get();
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
			SEBeginCPUEvent("en::EngineApp::Run frame outer loop");

			outerLoopTimer.Start();

			SEBeginCPUEvent("en::EngineApp::Update");
			EngineApp::Update(m_frameNum, lastOuterFrameTime);
			SEEndCPUEvent();

			// Update components until enough time has passed to trigger a render.
			// Or, continue rendering frames until it's time to update again
			elapsed += lastOuterFrameTime;
			while (elapsed >= m_fixedTimeStep)
			{	
				SEBeginCPUEvent("en::EngineApp::Run frame inner loop");

				elapsed -= m_fixedTimeStep;

				// Pump our events/input:
				SEBeginCPUEvent("en::EventManager::Update");
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
			SEBeginCPUEvent("en::EngineApp::Run Wait on copy step");
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
		SEBeginCPUEvent("en::EngineApp::Shutdown");

		LOG("EngineApp shutting down...");

		fr::UIManager::Get()->Shutdown();
		
		fr::EntityManager::Get()->Shutdown();

		fr::SceneManager::Get()->Shutdown();

		// We need to signal the render thread to shut down and wait on it to complete before we can start destroying
		// anything it might be using
		re::RenderManager::Get()->ThreadShutdown();

		en::InputManager::Get()->Shutdown();
		en::EventManager::Get()->Shutdown();

		m_window->Destroy();

		core::LogManager::Get()->Shutdown(); // Destroy last

		core::ThreadPool::Get()->Stop();

		SEEndCPUEvent();
	}

	
	void EngineApp::Update(uint64_t frameNum, double stepTimeMs)
	{
		SEBeginCPUEvent("en::EngineApp::Update");

		HandleEvents();

		SEEndCPUEvent();
	}


	void EngineApp::HandleEvents()
	{
		SEBeginCPUEvent("en::EngineApp::HandleEvents");

		while (HasEvents())
		{
			en::EventManager::EventInfo const& eventInfo = GetEvent();

			switch (eventInfo.m_type)
			{
			case en::EventManager::EngineQuit:
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