#include <string>

#include "CoreEngine.h"
#include "Config.h"
#include "DebugConfiguration.h"
#include "Platform.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "EventManager.h"
#include "InputManager.h"
#include "PerformanceTimer.h"

using en::Config;
using en::SceneManager;
using en::EventManager;
using en::InputManager;
using re::RenderManager;
using util::PerformanceTimer;
using std::shared_ptr;
using std::make_shared;
using std::string;


namespace en
{
	CoreEngine*	CoreEngine::m_coreEngine = nullptr;


	CoreEngine::CoreEngine(int argc, char** argv)
		: m_fixedTimeStep(1000.0 / 120.0)
		, m_isRunning(false)
		, m_logManager(make_shared<en::LogManager>())
	{
		m_coreEngine = this;

		if (!ProcessCommandLineArgs(argc, argv))
		{
			exit(-1);
		}
	}


	void CoreEngine::Startup()
	{
		LOG("CoreEngine starting...");

		// Start managers:
		EventManager::Get()->Startup();
		m_logManager->Startup();

		EventManager::Get()->Subscribe(en::EventManager::EngineQuit, this);

		RenderManager::Get()->Startup();	// Initializes SDL events and video subsystems

		// For whatever reason, this needs to be called after the SDL video subsystem (!) has been initialized:
		InputManager::Get()->Startup();

		// Must wait to start scene manager and load a scene until the renderer is called, since we need to initialize
		// the rendering context in the RenderManager before creating shaders
		SceneManager::Get()->Startup();

		// Now that the scene (and its materials/shaders) has been loaded, we can initialize the shaders
		RenderManager::Get()->Initialize();

		m_isRunning = true;
	}


	// Main game loop
	void CoreEngine::Run()
	{
		LOG("CoreEngine beginning main game loop!");

		// Process any events that might have occurred during startup:
		EventManager::Get()->Update(0.0);

		// Initialize game loop timing:
		double elapsed = (double)m_fixedTimeStep; // Ensure we pump Updates once before the 1st render

		PerformanceTimer outerLoopTimer;
		PerformanceTimer innerLoopTimer;
		double lastOuterFrameTime = 0.0;

		while (m_isRunning)
		{
			outerLoopTimer.Start();

			EventManager::Get()->Update(lastOuterFrameTime);
			InputManager::Get()->Update(lastOuterFrameTime);
			CoreEngine::Update(lastOuterFrameTime);
			m_logManager->Update(lastOuterFrameTime);

			// Update components until enough time has passed to trigger a render.
			// Or, continue rendering frames until it's time to update again
			elapsed += lastOuterFrameTime;			
			while (elapsed >= m_fixedTimeStep)
			{	
				elapsed -= m_fixedTimeStep;

				SceneManager::Get()->Update(m_fixedTimeStep); // Updates all of the scene objects
				// AI, physics, etc should also be pumped here (eventually)
			}

			RenderManager::Get()->Update(lastOuterFrameTime);

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
		InputManager::Get()->Shutdown();
		RenderManager::Get()->Shutdown();
		SceneManager::Get()->Shutdown();
		EventManager::Get()->Shutdown();
		m_logManager->Shutdown();
	}

	
	void CoreEngine::Update(const double stepTimeMs)
	{
		HandleEvents();

		// Generate a quit event if the quit button is pressed:
		if (InputManager::Get()->GetKeyboardInputState(en::InputButton_Quit) == true)
		{
			EventManager::Get()->Notify(en::EventManager::EventInfo{en::EventManager::EngineQuit});
		}
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
			LOG_ERROR("No command line arguments received! Use \"-scene <scene name>\" to launch a scene from the .\\Scenes directory.\n\n\t\tEg. \tSaberEngine.exe -scene Sponza\n\nNote: The scene directory name and scene .FBX file must be the same");
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

					const string scenesRoot = Config::Get()->GetValue<string>("scenesRoot"); // "..\Scenes\"

					// From param of the form "Scene\Folder\Names\sceneFile.extension", we extract:

					// sceneFilePath == "..\Scenes\Scene\Folder\Names\sceneFile.extension":
					const string sceneFilePath = scenesRoot + sceneNameParam; 
					Config::Get()->SetValue("sceneFilePath", sceneFilePath, Config::SettingType::Runtime);

					// sceneRootPath == "..\Scenes\Scene\Folder\Names\":
					const size_t lastSlash = sceneFilePath.find_last_of("\\");
					const string sceneRootPath = sceneFilePath.substr(0, lastSlash) + "\\"; 
					Config::Get()->SetValue("sceneRootPath", sceneRootPath, Config::SettingType::Runtime);

					// sceneName == "sceneFile"
					const string filenameAndExt = sceneFilePath.substr(lastSlash + 1, sceneFilePath.size() - lastSlash);
					const size_t extensionPeriod = filenameAndExt.find_last_of(".");
					const string sceneName = filenameAndExt.substr(0, extensionPeriod);
					Config::Get()->SetValue("sceneName", sceneName, Config::SettingType::Runtime);

					// sceneIBLPath == "..\Scenes\SceneFolderName\IBL\ibl.hdr"
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