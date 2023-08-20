// © 2022 Adam Badke. All rights reserved.
#include <pix3.h>

#include "CoreEngine.h"
#include "Config.h"
#include "DebugConfiguration.h"
#include "Platform.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "EventManager.h"
#include "InputManager.h"
#include "PerformanceTimer.h"
#include "GameplayManager.h"
#include "LogManager.h"

using en::Config;
using en::SceneManager;
using en::EventManager;
using en::InputManager;
using en::LogManager;
using re::RenderManager;
using fr::GameplayManager;
using util::PerformanceTimer;
using std::shared_ptr;
using std::make_shared;
using std::string;


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
	}


	void CoreEngine::Startup()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::CoreEngine::Startup");

		LOG("CoreEngine starting...");

		m_threadPool.Startup();

		// Create a window:
		std::string commandLineArgs;
		Config::Get()->TryGetValue<string>(en::ConfigKeys::k_commandLineArgsValueName, commandLineArgs);

		const string windowTitle = Config::Get()->GetValue<string>("windowTitle") + " " + commandLineArgs;
		const int xRes = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowXResValueName);
		const int yRes = Config::Get()->GetValue<int>(en::ConfigKeys::k_windowYResValueName);

		m_window = std::make_unique<en::Window>(); // Ensure Window exists for first callbacks triggered by Create
		const bool windowCreated = m_window->Create(windowTitle, xRes, yRes);
		SEAssert("Failed to create a window", windowCreated);

		// Don't capture the mouse while we're loading
		m_window->SetRelativeMouseMode(false);

		// Render thread:
		re::RenderManager* renderManager = RenderManager::Get();
		m_threadPool.EnqueueJob([&]() {renderManager->Lifetime(m_copyBarrier.get()); });
		renderManager->ThreadStartup(); // Initializes context

		// Start managers:
		en::EventManager* eventManager = en::EventManager::Get();
		eventManager->Startup();
		eventManager->Subscribe(en::EventManager::EngineQuit, this);

		LogManager::Get()->Startup();
		InputManager::Get()->Startup(); // Now that the window is created

		SceneManager::Get()->Startup(); // Load assets

		renderManager->ThreadInitialize(); // Create graphics systems, close PB registration

		// Create gameplay objects now that the scene data is loaded
		GameplayManager::Get()->Startup();

		m_isRunning = true;

		// We're done loading: Capture the mouse
		m_window->SetRelativeMouseMode(true);

		PIXEndEvent();
	}


	// Main game loop
	void CoreEngine::Run()
	{
		LOG("\nCoreEngine: Starting main game loop\n");

		en::EventManager* eventManager = EventManager::Get();
		en::LogManager* logManager = LogManager::Get();
		en::InputManager* inputManager = InputManager::Get();
		fr::GameplayManager* gameplayManager = GameplayManager::Get();
		en::SceneManager* sceneManager = SceneManager::Get();
		re::RenderManager* renderManager = RenderManager::Get();

		// Process any events that might have occurred during startup:
		eventManager->Update(m_frameNum, 0.0);

		// Initialize game loop timing:
		double elapsed = (double)m_fixedTimeStep; // Ensure we pump Updates once before the 1st render

		PerformanceTimer outerLoopTimer;
		PerformanceTimer innerLoopTimer;
		double lastOuterFrameTime = 0.0;

		while (m_isRunning)
		{
			PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::CoreEngine::Run frame outer loop");

			outerLoopTimer.Start();

			PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::CoreEngine::Update");
			CoreEngine::Update(m_frameNum, lastOuterFrameTime);
			PIXEndEvent();

			PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::LogManager::Update");
			logManager->Update(m_frameNum, lastOuterFrameTime);
			PIXEndEvent();

			// Update components until enough time has passed to trigger a render.
			// Or, continue rendering frames until it's time to update again
			elapsed += lastOuterFrameTime;
			while (elapsed >= m_fixedTimeStep)
			{	
				PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::CoreEngine::Run frame inner loop");

				elapsed -= m_fixedTimeStep;

				// Pump our events/input:
				PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::EventManager::Update");
				eventManager->Update(m_frameNum, lastOuterFrameTime);
				PIXEndEvent();

				PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::InputManager::Update");
				inputManager->Update(m_frameNum, lastOuterFrameTime);
				PIXEndEvent();

				// Pump systems that rely on events/input:
				PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::GameplayManager::Update");
				gameplayManager->Update(m_frameNum, m_fixedTimeStep);
				PIXEndEvent();

				PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::SceneManager::Update");
				sceneManager->Update(m_frameNum, m_fixedTimeStep); // Updates all of the scene objects
				PIXEndEvent();

				// AI, physics, etc should also be pumped here (eventually)

				PIXEndEvent();
			}

			PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::SceneManager::FinalUpdate");
			sceneManager->FinalUpdate(); // Builds batches, ready for RenderManager to consume
			PIXEndEvent();

			// Pump the render thread, and wait for it to signal copying is complete:
			PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::CoreEngine::Run Wait on copy barrier");
			renderManager->EnqueueUpdate({m_frameNum, lastOuterFrameTime});
			m_copyBarrier->arrive_and_wait();
			PIXEndEvent();

			++m_frameNum;

			lastOuterFrameTime = outerLoopTimer.StopMs();

			PIXEndEvent();
		}
	}


	void CoreEngine::Stop()
	{
		m_isRunning = false;
	}


	void CoreEngine::Shutdown()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::CoreEngine::Shutdown");

		LOG("CoreEngine shutting down...");

		Config::Get()->SaveConfigFile();
		
		// Note: Shutdown order matters!
		GameplayManager::Get()->Shutdown();
		InputManager::Get()->Shutdown();

		RenderManager::Get()->ThreadShutdown();

		SceneManager::Get()->Shutdown();
		EventManager::Get()->Shutdown();
		LogManager::Get()->Shutdown();

		m_window->Destroy();

		m_threadPool.Stop();

		PIXEndEvent();
	}

	
	void CoreEngine::Update(uint64_t frameNum, double stepTimeMs)
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::CoreEngine::Update");

		HandleEvents();

		PIXEndEvent();
	}


	void CoreEngine::HandleEvents()
	{
		PIXBeginEvent(PIX_COLOR_INDEX(PIX_FORMAT_COLOR::CPUSection), "en::CoreEngine::HandleEvents");

		while (HasEvents())
		{
			en::EventManager::EventInfo eventInfo = GetEvent();

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

		PIXEndEvent();
	}
}