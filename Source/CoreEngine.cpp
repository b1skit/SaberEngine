// © 2022 Adam Badke. All rights reserved.
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


	CoreEngine::CoreEngine(int argc, char** argv)
		: m_fixedTimeStep(1000.0 / 120.0)
		, m_isRunning(false)
		, m_frameNum(0)
		, m_window(nullptr)
	{
		m_coreEngine = this;

		if (!ProcessCommandLineArgs(argc, argv))
		{
			exit(-1);
		}

		m_copyBarrier = std::make_unique<std::barrier<>>(k_numSystemThreads);
	}


	void CoreEngine::Startup()
	{
		LOG("CoreEngine starting...");

		m_threadPool.Startup();

		// Create a window:
		const string windowTitle = Config::Get()->GetValue<string>("windowTitle") + " " +
			Config::Get()->GetValue<string>("commandLineArgs");
		const int xRes = Config::Get()->GetValue<int>("windowXRes");
		const int yRes = Config::Get()->GetValue<int>("windowYRes");

		m_window = std::make_unique<re::Window>(); // Ensure Window exists for first callbacks triggered by Create
		const bool windowCreated = m_window->Create(windowTitle, xRes, yRes);
		SEAssert("Failed to create a window", windowCreated);

		// Start managers:
		EventManager::Get()->Startup();
		EventManager::Get()->Subscribe(en::EventManager::EngineQuit, this);

		LogManager::Get()->Startup();
		InputManager::Get()->Startup(); // Now that the window is created

		// Render thread:
		m_threadPool.EnqueueJob([&]() {RenderManager::Get()->Lifetime(m_copyBarrier.get()); });
		RenderManager::Get()->ThreadStartup(); // Initializes context

		SceneManager::Get()->Startup(); // Load assets

		RenderManager::Get()->ThreadInitialize(); // Create graphics systems

		// Create gameplay objects now that the scene data is loaded
		GameplayManager::Get()->Startup();

		m_isRunning = true;
	}


	// Main game loop
	void CoreEngine::Run()
	{
		LOG("\nCoreEngine: Starting main game loop\n");

		m_window->SetRelativeMouseMode(true);

		// Process any events that might have occurred during startup:
		EventManager::Get()->Update(m_frameNum, 0.0);

		// Initialize game loop timing:
		double elapsed = (double)m_fixedTimeStep; // Ensure we pump Updates once before the 1st render

		PerformanceTimer outerLoopTimer;
		PerformanceTimer innerLoopTimer;
		double lastOuterFrameTime = 0.0;

		while (m_isRunning)
		{
			outerLoopTimer.Start();

			EventManager::Get()->Update(m_frameNum, lastOuterFrameTime);
			InputManager::Get()->Update(m_frameNum, lastOuterFrameTime);
			CoreEngine::Update(m_frameNum, lastOuterFrameTime);
			LogManager::Get()->Update(m_frameNum, lastOuterFrameTime);

			// Update components until enough time has passed to trigger a render.
			// Or, continue rendering frames until it's time to update again
			elapsed += lastOuterFrameTime;			
			while (elapsed >= m_fixedTimeStep)
			{	
				elapsed -= m_fixedTimeStep;

				GameplayManager::Get()->Update(m_frameNum, m_fixedTimeStep);
				SceneManager::Get()->Update(m_frameNum, m_fixedTimeStep); // Updates all of the scene objects
				// AI, physics, etc should also be pumped here (eventually)
			}

			SceneManager::Get()->FinalUpdate(); // Builds batches, ready for RenderManager to consume

			// Pump the render thread, and wait for it to signal copying is complete:
			RenderManager::Get()->EnqueueUpdate({m_frameNum, lastOuterFrameTime});
			m_copyBarrier->arrive_and_wait();

			++m_frameNum;

			lastOuterFrameTime = outerLoopTimer.StopMs();
		}
	}


	void CoreEngine::Stop()
	{
		m_isRunning = false;
	}


	void CoreEngine::Shutdown()
	{
		LOG("CoreEngine shutting down...");

		Config::Get()->SaveConfig();
		
		// Note: Shutdown order matters!
		GameplayManager::Get()->Shutdown();
		InputManager::Get()->Shutdown();

		RenderManager::Get()->ThreadShutdown();

		SceneManager::Get()->Shutdown();
		EventManager::Get()->Shutdown();
		LogManager::Get()->Shutdown();

		m_window->Destroy();

		m_threadPool.Stop();
	}

	
	void CoreEngine::Update(uint64_t frameNum, double stepTimeMs)
	{
		HandleEvents();
	}


	void CoreEngine::HandleEvents()
	{
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
	}


	bool CoreEngine::ProcessCommandLineArgs(int argc, char** argv)
	{
		if (argc <= 1)
		{
			LOG_ERROR("No command line arguments received! Use \"-scene <scene name>\" to launch a scene from the "
				".\\Scenes directory.\n\n\t\tEg. \tSaberEngine.exe -scene Sponza\n\nNote: The scene directory name and "
				"scene .FBX file must be the same");
			return false;
		}
		const int numTokens = argc - 1; // -1, as 1st arg is program name
		LOG("Processing %d command line tokens...", numTokens);

		bool successfulParse = true;

		string argString; // The full list of all command line args received

		for (int i = 1; i < argc; i++)
		{			
			const string currentArg(argv[i]);
			
			argString += currentArg + (i < (argc - 1) ? " " : "");

			if (currentArg.find("-scene") != string::npos)
			{
				if (i < argc - 1) // -1 as we need to peek ahead
				{
					const int nextArg = i + 1;
					const string sceneNameParam = string(argv[nextArg]);

					argString += sceneNameParam;

					LOG("\tReceived scene command: \"%s %s\"", currentArg.c_str(), sceneNameParam.c_str());

					const string scenesRoot = Config::Get()->GetValue<string>("scenesRoot"); // ".\Scenes\"

					// From param of the form "Scene\Folder\Names\sceneFile.extension", we extract:

					// sceneFilePath == ".\Scenes\Scene\Folder\Names\sceneFile.extension":
					const string sceneFilePath = scenesRoot + sceneNameParam; 
					Config::Get()->SetValue("sceneFilePath", sceneFilePath, Config::SettingType::Runtime);

					// sceneRootPath == ".\Scenes\Scene\Folder\Names\":
					const size_t lastSlash = sceneFilePath.find_last_of("\\");
					const string sceneRootPath = sceneFilePath.substr(0, lastSlash) + "\\"; 
					Config::Get()->SetValue("sceneRootPath", sceneRootPath, Config::SettingType::Runtime);

					// sceneName == "sceneFile"
					const string filenameAndExt = sceneFilePath.substr(lastSlash + 1, sceneFilePath.size() - lastSlash);
					const size_t extensionPeriod = filenameAndExt.find_last_of(".");
					const string sceneName = filenameAndExt.substr(0, extensionPeriod);
					Config::Get()->SetValue("sceneName", sceneName, Config::SettingType::Runtime);

					// sceneIBLPath == ".\Scenes\SceneFolderName\IBL\ibl.hdr"
					const string sceneIBLPath = sceneRootPath + "IBL\\ibl.hdr";
					Config::Get()->SetValue("sceneIBLPath", sceneIBLPath, Config::SettingType::Runtime);
				}
				else
				{
					LOG_ERROR("Received \"-scene\" token, but no matching scene name");
					successfulParse = false;
				}
				
				i++; // Consume the token
			}
			else
			{
				LOG_ERROR("\"%s\" is not a recognized command!", currentArg.c_str());
				successfulParse = false;
			}
		}

		// Store the received command line string
		Config::Get()->SetValue("commandLineArgs", argString, Config::SettingType::Runtime);

		SEAssert("Command line argument parsing failed", successfulParse);		

		return successfulParse;
	}
}