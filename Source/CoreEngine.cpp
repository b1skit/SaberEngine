// © 2022 Adam Badke. All rights reserved.
#include "CoreEngine.h"
#include "Config.h"
#include "Assert.h"
#include "Platform.h"
#include "ProfilingMarkers.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "EventManager.h"
#include "InputManager.h"
#include "PerformanceTimer.h"
#include "GameplayManager.h"
#include "LogManager.h"


namespace
{
	constexpr size_t k_numSystemThreads = 2;
}

namespace en
{
	CoreEngine*	CoreEngine::m_coreEngine = nullptr;


	CoreEngine::CoreEngine()
		: m_fixedTimeStep(1000.0 / 120.0) // 1000/120 = 8.33ms per update
		, m_isRunning(false)
		, m_frameNum(0)
		, m_window(nullptr)
	{
		m_coreEngine = this;
		m_copyBarrier = std::make_unique<std::barrier<>>(k_numSystemThreads);
		en::ThreadPool::NameCurrentThread(L"Main Thread");
	}


	void CoreEngine::Startup()
	{
		SEBeginCPUEvent("en::CoreEngine::Startup");

		LOG("CoreEngine starting...");

		m_threadPool.Startup();

		// Create a window:
		std::string commandLineArgs;
		Config::Get()->TryGetValue<std::string>(en::ConfigKeys::k_commandLineArgsValueKey, commandLineArgs);

		const std::string windowTitle = Config::Get()->GetValue<std::string>("windowTitle") + " " + commandLineArgs;
		const int xRes = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowWidthKey);
		const int yRes = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowHeightKey);

		m_window = std::make_unique<en::Window>(); // Ensure Window exists for first callbacks triggered by Create
		const bool windowCreated = m_window->Create(windowTitle, xRes, yRes);
		SEAssert("Failed to create a window", windowCreated);

		// Don't capture the mouse while we're loading
		m_window->SetRelativeMouseMode(false);

		// Render thread:
		re::RenderManager* renderManager = re::RenderManager::Get();
		m_threadPool.EnqueueJob([&]()
			{
				en::ThreadPool::NameCurrentThread(L"Render Thread");
				renderManager->Lifetime(m_copyBarrier.get()); 
			});
		renderManager->ThreadStartup(); // Initializes context

		// Start managers:
		en::EventManager* eventManager = en::EventManager::Get();
		eventManager->Startup();
		eventManager->Subscribe(en::EventManager::EngineQuit, this);

		LogManager::Get()->Startup();
		InputManager::Get()->Startup(); // Now that the window is created

		SceneManager::Get()->Startup(); // Load assets

		// Create gameplay objects now that the scene data is loaded
		fr::GameplayManager::Get()->Startup();

		renderManager->ThreadInitialize(); // Create render systems, close PB registration

		m_isRunning = true;

		// We're done loading: Capture the mouse
		m_window->SetRelativeMouseMode(true);

		SEEndCPUEvent();
	}


	// Main game loop
	void CoreEngine::Run()
	{
		LOG("\nCoreEngine: Starting main game loop\n");

		en::EventManager* eventManager = EventManager::Get();
		en::LogManager* logManager = LogManager::Get();
		en::InputManager* inputManager = InputManager::Get();
		fr::GameplayManager* gameplayManager = fr::GameplayManager::Get();
		en::SceneManager* sceneManager = SceneManager::Get();
		re::RenderManager* renderManager = re::RenderManager::Get();

		// Process any events that might have occurred during startup:
		eventManager->Update(m_frameNum, 0.0);

		// Initialize game loop timing:
		double elapsed = (double)m_fixedTimeStep; // Ensure we pump Updates once before the 1st render

		util::PerformanceTimer outerLoopTimer;
		util::PerformanceTimer innerLoopTimer;
		double lastOuterFrameTime = 0.0;

		while (m_isRunning)
		{
			SEBeginCPUEvent("en::CoreEngine::Run frame outer loop");

			outerLoopTimer.Start();

			SEBeginCPUEvent("en::CoreEngine::Update");
			CoreEngine::Update(m_frameNum, lastOuterFrameTime);
			SEEndCPUEvent();

			SEBeginCPUEvent("en::LogManager::Update");
			logManager->Update(m_frameNum, lastOuterFrameTime);
			SEEndCPUEvent();

			// Update components until enough time has passed to trigger a render.
			// Or, continue rendering frames until it's time to update again
			elapsed += lastOuterFrameTime;
			while (elapsed >= m_fixedTimeStep)
			{	
				SEBeginCPUEvent("en::CoreEngine::Run frame inner loop");

				elapsed -= m_fixedTimeStep;

				// Pump our events/input:
				SEBeginCPUEvent("en::EventManager::Update");
				eventManager->Update(m_frameNum, lastOuterFrameTime);
				SEEndCPUEvent();

				SEBeginCPUEvent("en::InputManager::Update");
				inputManager->Update(m_frameNum, lastOuterFrameTime);
				SEEndCPUEvent();

				// Pump systems that rely on events/input:
				SEBeginCPUEvent("en::GameplayManager::Update");
				gameplayManager->Update(m_frameNum, m_fixedTimeStep);
				SEEndCPUEvent();

				SEBeginCPUEvent("en::SceneManager::Update");
				sceneManager->Update(m_frameNum, m_fixedTimeStep); // Updates all of the scene objects
				SEEndCPUEvent();

				// AI, physics, etc should also be pumped here (eventually)

				SEEndCPUEvent();
			}

			SEBeginCPUEvent("fr::GameplayManager::EnqueueRenderUpdates");
			gameplayManager->EnqueueRenderUpdates();
			SEEndCPUEvent();


			// DEPRECATED:
			SEBeginCPUEvent("en::SceneManager::FinalUpdate");
			sceneManager->FinalUpdate(); // Builds batches, ready for RenderManager to consume
			SEEndCPUEvent();


			// Pump the render thread, and wait for it to signal copying is complete:
			SEBeginCPUEvent("en::CoreEngine::Run Wait on copy barrier");
			renderManager->EnqueueUpdate({m_frameNum, lastOuterFrameTime});
			m_copyBarrier->arrive_and_wait();
			SEEndCPUEvent();

			++m_frameNum;

			lastOuterFrameTime = outerLoopTimer.StopMs();

			SEEndCPUEvent();
		}
	}


	void CoreEngine::Stop()
	{
		m_isRunning = false;
	}


	void CoreEngine::Shutdown()
	{
		SEBeginCPUEvent("en::CoreEngine::Shutdown");

		LOG("CoreEngine shutting down...");

		Config::Get()->SaveConfigFile();
		
		// We need to signal the render thread to shut down and wait on it to complete before we can start destroying
		// anything it might be using
		re::RenderManager::Get()->ThreadShutdown();

		// Note: Shutdown order matters!
		InputManager::Get()->Shutdown();

		SceneManager::Get()->Shutdown();

		fr::GameplayManager::Get()->Shutdown(); // TODO: This should happen BEFORE RenderManager ThreadShutdown

		EventManager::Get()->Shutdown();
		LogManager::Get()->Shutdown();

		m_window->Destroy();

		m_threadPool.Stop();

		SEEndCPUEvent();
	}

	
	void CoreEngine::Update(uint64_t frameNum, double stepTimeMs)
	{
		SEBeginCPUEvent("en::CoreEngine::Update");

		HandleEvents();

		SEEndCPUEvent();
	}


	void CoreEngine::HandleEvents()
	{
		SEBeginCPUEvent("en::CoreEngine::HandleEvents");

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