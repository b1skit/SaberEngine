// © 2022 Adam Badke. All rights reserved.
#include "EngineApp.h"

#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/EventManager.h"
#include "Core/InputManager.h"
#include "Core/Logger.h"
#include "Core/PerfLogger.h"
#include "Core/ProfilingMarkers.h"
#include "Core/ThreadPool.h"

#include "Core/Host/PerformanceTimer.h"
#include "Core/Host/Window.h"

#include "Presentation/EntityManager.h"
#include "Presentation/SceneManager.h"
#include "Presentation/UIManager.h"

#include "Renderer/Context.h"
#include "Renderer/RenderManager.h"


namespace
{
	constexpr size_t k_numSystemThreads = 2;

	constexpr util::CHashKey k_mainThreadLoggerKey("Main thread");


	// Create the main window on the engine thread to associate it with the correct Win32 event queue
	void InitializeAppWindow(host::Window* appWindow, bool allowDragAndDrop)
	{
		std::string commandLineArgs;
		core::Config::Get()->TryGetValue<std::string>(core::configkeys::k_commandLineArgsValueKey, commandLineArgs);

		std::string const& windowTitle = std::format("{} {}",
			core::Config::Get()->GetValue<std::string>("windowTitle"),
			commandLineArgs);
		const int xRes = core::Config::Get()->GetValue<int>(core::configkeys::k_windowWidthKey);
		const int yRes = core::Config::Get()->GetValue<int>(core::configkeys::k_windowHeightKey);

		const host::Window::CreateParams createParams
		{
			.m_title = windowTitle,
			.m_width = util::CheckedCast<uint32_t>(xRes),
			.m_height = util::CheckedCast<uint32_t>(yRes),
			.m_allowDragAndDrop = allowDragAndDrop,
		};

		const bool windowCreated = appWindow->Create(createParams);
		SEAssert(windowCreated, "Failed to create a window");
	}
}

namespace app
{
	EngineApp* EngineApp::m_engineApp = nullptr;


	EngineApp::EngineApp()
		: m_isRunning(false)
		, m_frameNum(0)
		, m_window(std::make_unique<host::Window>())
		, m_inventory(std::make_unique<core::Inventory>())
	{
		m_engineApp = this;

		m_syncBarrier = std::make_unique<std::barrier<>>(k_numSystemThreads);
		core::ThreadPool::NameCurrentThread(L"Main Thread");
	}


	void EngineApp::Startup()
	{
		SEBeginCPUEvent("app::EngineApp::Startup");

		LOG("EngineApp starting...");

		core::EventManager* eventManager = core::EventManager::Get();
		eventManager->Startup();

		eventManager->Subscribe(eventkey::EngineQuit, this);

		core::Config::Get()->ProcessCommandLineArgs();

		// Show the console if requested now that we've parsed the command line args
		const bool showConsole = core::Config::Get()->KeyExists(core::configkeys::k_showSystemConsoleWindowCmdLineArg);
		if (showConsole)
		{
			AllocConsole();
			freopen("CONOUT$", "wb", stdout);
		}

		// Stand up critical systems first:
		core::ThreadPool::Get()->Startup();

		// Start the logging thread:
		core::Logger::Get()->Startup(
			core::Config::Get()->KeyExists(core::configkeys::k_showSystemConsoleWindowCmdLineArg));

		// Create a window (and interally pass it to the re::Context)
		constexpr bool k_allowDragAndDrop = true; // Allways allowed, for now
		InitializeAppWindow(m_window.get(), k_allowDragAndDrop);

		re::RenderManager* renderManager = re::RenderManager::Get();
		fr::EntityManager* entityMgr = fr::EntityManager::Get();
		fr::SceneManager* sceneMgr = fr::SceneManager::Get();
		fr::UIManager* uiMgr = fr::UIManager::Get();

		// Dependency injection:
		entityMgr->SetInventory(m_inventory.get()); 
		renderManager->SetInventory(m_inventory.get());
		sceneMgr->SetInventory(m_inventory.get());

		renderManager->SetWindow(m_window.get());
		uiMgr->SetWindow(m_window.get());

		// Render thread:
		core::ThreadPool::Get()->EnqueueJob([&]()
			{
				core::ThreadPool::NameCurrentThread(L"Render Thread");
				renderManager->Lifetime(m_syncBarrier.get()); 
			});
		renderManager->ThreadStartup(); // Initializes context
		
		en::InputManager::Get()->Startup(); // Now that the window is created

		sceneMgr->Startup();

		entityMgr->Startup();

		renderManager->ThreadInitialize();

		uiMgr->Startup();

		core::PerfLogger::Get()->Register(k_mainThreadLoggerKey);

		m_isRunning = true;

		SEEndCPUEvent();
	}


	// Main game loop
	void EngineApp::Run()
	{
		LOG("\nEngineApp: Starting main game loop\n");

		core::EventManager* eventManager = core::EventManager::Get();
		core::Logger* logger = core::Logger::Get();
		en::InputManager* inputManager = en::InputManager::Get();
		fr::EntityManager* entityManager = fr::EntityManager::Get();
		fr::SceneManager* sceneManager = fr::SceneManager::Get();
		re::RenderManager* renderManager = re::RenderManager::Get();
		fr::UIManager* uiManager = fr::UIManager::Get();

		core::PerfLogger* perfLogger = core::PerfLogger::Get();

		// Process any events that might have occurred during startup:
		eventManager->Update(m_frameNum, 0.0);

		// Initialize game loop timing:
		double elapsed = k_fixedTimeStep; // Ensure we pump Updates once before the 1st render

		host::PerformanceTimer outerLoopTimer;
		host::PerformanceTimer innerLoopTimer;
		double lastOuterFrameTime = 0.0;

		while (m_isRunning)
		{
			SEBeginCPUEvent("app::EngineApp::Run frame outer loop");

			// Get the total time taken to reach this point from the previous frame:
			if (outerLoopTimer.IsRunning()) // Not running if this is the 1st frame
			{
				lastOuterFrameTime = outerLoopTimer.StopMs();
			}
			outerLoopTimer.Start();

			perfLogger->NotifyBegin(k_mainThreadLoggerKey);

			SEBeginCPUEvent("app::EngineApp::Update");
			EngineApp::Update(m_frameNum, lastOuterFrameTime);
			SEEndCPUEvent();

			// Update components until enough time has passed to trigger a render.
			// Or, continue rendering frames until it's time to update again.			
			elapsed += std::min(lastOuterFrameTime, k_maxOuterFrameTime);
			while (elapsed >= k_fixedTimeStep)
			{	
				SEBeginCPUEvent("app::EngineApp::Run frame inner loop");

				elapsed -= k_fixedTimeStep;

				// Pump our events/input:
				SEBeginCPUEvent("core::EventManager::Update");
				eventManager->Update(m_frameNum, k_fixedTimeStep);
				SEEndCPUEvent();

				SEBeginCPUEvent("en::InputManager::Update");
				inputManager->Update(m_frameNum, k_fixedTimeStep);
				SEEndCPUEvent();

				SEBeginCPUEvent("en::EntityManager::Update");
				entityManager->Update(m_frameNum, k_fixedTimeStep);
				SEEndCPUEvent();

				SEEndCPUEvent();
			}

			SEBeginCPUEvent("fr::SceneManager::Update");
			sceneManager->Update(m_frameNum, lastOuterFrameTime); // Note: Must be updated after entity manager (e.g. Reset)
			SEEndCPUEvent();

			SEBeginCPUEvent("fr::UIManager::Update");
			uiManager->Update(m_frameNum, lastOuterFrameTime);
			SEEndCPUEvent();

			SEBeginCPUEvent("fr::EntityManager::EnqueueRenderUpdates");
			entityManager->EnqueueRenderUpdates();
			SEEndCPUEvent();

			m_inventory->OnEndOfFrame(); // Free Resources that have gone out of scope

			// Pump the render thread:
			renderManager->EnqueueUpdate({ m_frameNum, lastOuterFrameTime });

			++m_frameNum;

			perfLogger->NotifyEnd(k_mainThreadLoggerKey);

			// Wait for the render thread to begin processing the current frame before we proceed to the next one:
			SEBeginCPUEvent("app::EngineApp::Run Wait on render thread");
			m_syncBarrier->arrive_and_wait();
			SEEndCPUEvent();

			SEEndCPUEvent();
		}

		if (outerLoopTimer.IsRunning())
		{
			outerLoopTimer.StopMs();
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
		// anything it might be using.
		// Note: The RenderManager destroys the Inventory via the pointer we gave it to ensure render objects are
		// destroyed on the main render thread (as required by OpenGL)
		re::RenderManager::Get()->ThreadShutdown();

		en::InputManager::Get()->Shutdown();
		core::EventManager::Get()->Shutdown();

		core::Logger::Get()->Shutdown(); // Destroy last

		core::ThreadPool::Get()->Stop();
		
		m_window->Destroy();

		// Finally, close the console if it was opened:
		if (core::Config::Get()->KeyExists(core::configkeys::k_showSystemConsoleWindowCmdLineArg))
		{
			FreeConsole();
			fclose(stdout);
		}

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

			switch (eventInfo.m_eventKey)
			{
			case eventkey::EngineQuit:
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