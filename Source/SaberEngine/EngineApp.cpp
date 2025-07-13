// © 2022 Adam Badke. All rights reserved.
#include "EngineApp.h"
#include "Platform.h"

#include "Core/AccessKey.h"
#include "Core/Assert.h"
#include "Core/Config.h"
#include "Core/EventManager.h"
#include "Core/InputManager.h"
#include "Core/Logger.h"
#include "Core/PerfLogger.h"
#include "Core/ProfilingMarkers.h"
#include "Core/ThreadPool.h"

#include "Core/Definitions/EventKeys.h"

#include "Core/Host/PerformanceTimer.h"
#include "Core/Host/Window.h"

#include "Presentation/EntityManager.h"
#include "Presentation/SceneManager.h"
#include "Presentation/UIManager.h"

#include "Renderer/RenderManager.h"


namespace
{
	constexpr size_t k_numSystemThreads = 2;

	constexpr char const* k_mainThreadLoggerName = "Main thread";


	// Create the main window on the engine thread to associate it with the correct Win32 event queue
	void InitializeAppWindow(host::Window* appWindow, bool allowDragAndDrop)
	{
		std::string commandLineArgs;
		core::Config::TryGetValue<std::string>(core::configkeys::k_commandLineArgsValueKey, commandLineArgs);

		std::string const& windowTitle = std::format("{} {}",
			core::Config::GetValue<std::string>("windowTitle"),
			commandLineArgs);
		const int xRes = core::Config::GetValue<int>(core::configkeys::k_windowWidthKey);
		const int yRes = core::Config::GetValue<int>(core::configkeys::k_windowHeightKey);

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
	{
		m_engineApp = this;

		m_syncBarrier = std::make_unique<std::barrier<>>(k_numSystemThreads);
		core::ThreadPool::NameCurrentThread(L"Main Thread");
	}


	void EngineApp::Startup()
	{
		SEBeginCPUEvent("app::EngineApp::Startup");

		LOG("EngineApp starting...");

		core::Config::ProcessCommandLineArgs();

		// Create the RenderManager immediately after processing the command line args, as it needs to set the
		// platform::RenderingAPI in the Config before we bind the platform functions
		gr::RenderManager* renderManager = gr::RenderManager::Get();

		// Register our API-specific bindings before anything attempts to call them:
		if (!platform::RegisterPlatformFunctions())
		{
			LOG_ERROR("Failed to configure API-specific platform bindings!\n");
			exit(-1);
		}

		core::EventManager* eventManager = core::EventManager::Get();
		eventManager->Startup();

		eventManager->Subscribe(eventkey::EngineQuit, this);

		// Show the console if requested now that we've parsed the command line args
		const bool showConsole = core::Config::KeyExists(core::configkeys::k_showSystemConsoleWindowCmdLineArg);
		if (showConsole)
		{
			AllocConsole();
			freopen("CONOUT$", "wb", stdout);
		}

		// Stand up critical systems first:
		core::ThreadPool::Get()->Startup();

		// Start the logging thread:
		core::Logger::Get()->Startup(
			core::Config::KeyExists(core::configkeys::k_showSystemConsoleWindowCmdLineArg));

		// Create a window (and internally pass it to the re::Context)
		constexpr bool k_allowDragAndDrop = true; // Always allowed, for now
		InitializeAppWindow(m_window.get(), k_allowDragAndDrop);

		pr::EntityManager* entityMgr = pr::EntityManager::Get();
		pr::SceneManager* sceneMgr = pr::SceneManager::Get();
		pr::UIManager* uiMgr = pr::UIManager::Get();

		// Dependency injection:
		renderManager->SetWindow(m_window.get());
		uiMgr->SetWindow(m_window.get());
		uiMgr->SetRenderManager(renderManager);

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
		pr::EntityManager* entityManager = pr::EntityManager::Get();
		pr::SceneManager* sceneManager = pr::SceneManager::Get();
		gr::RenderManager* renderManager = gr::RenderManager::Get();
		pr::UIManager* uiManager = pr::UIManager::Get();

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

			perfLogger->BeginFrame();
			perfLogger->NotifyBegin(k_mainThreadLoggerName);

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

			SEBeginCPUEvent("pr::SceneManager::Update");
			sceneManager->Update(m_frameNum, lastOuterFrameTime); // Note: Must be updated after entity manager (e.g. Reset)
			SEEndCPUEvent();

			SEBeginCPUEvent("pr::UIManager::Update");
			uiManager->Update(m_frameNum, lastOuterFrameTime);
			SEEndCPUEvent();

			SEBeginCPUEvent("pr::EntityManager::EnqueueRenderUpdates");
			entityManager->EnqueueRenderUpdates();
			SEEndCPUEvent();

			// Pump the render thread:
			renderManager->EnqueueUpdate({ m_frameNum, lastOuterFrameTime });

			++m_frameNum;

			perfLogger->NotifyEnd(k_mainThreadLoggerName);

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

		core::Config::SaveConfigFile();

		pr::UIManager::Get()->Shutdown();
		
		pr::EntityManager::Get()->Shutdown();

		pr::SceneManager::Get()->Shutdown();

		// We need to signal the render thread to shut down and wait on it to complete before we can start destroying
		// anything it might be using.
		// Note: The RenderManager destroys the Inventory via the pointer we gave it to ensure render objects are
		// destroyed on the main render thread (as required by OpenGL)
		gr::RenderManager::Get()->ThreadShutdown();

		en::InputManager::Get()->Shutdown();
		core::EventManager::Get()->Shutdown();

		core::Logger::Get()->Shutdown(); // Destroy last

		core::ThreadPool::Get()->Stop();
		
		m_window->Destroy();

		// Finally, close the console if it was opened:
		if (core::Config::KeyExists(core::configkeys::k_showSystemConsoleWindowCmdLineArg))
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