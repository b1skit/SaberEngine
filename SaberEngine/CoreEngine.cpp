#include <string>

#include "CoreEngine.h"
#include "Config.h"
#include "DebugConfiguration.h"
#include "Platform.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include "EventManager.h"
#include "InputManager.h"

using en::Config;
using en::SceneManager;
using en::EventManager;
using en::InputManager;
using re::RenderManager;
using std::shared_ptr;
using std::make_shared;
using std::string;


namespace en
{
	CoreEngine*			CoreEngine::m_coreEngine = nullptr;


	CoreEngine::CoreEngine(int argc, char** argv) :
		m_FixedTimeStep(1000.0 / 120.0),
		m_isRunning(false),
		m_logManager(make_shared<en::LogManager>()),
		m_timeManager(make_shared<en::TimeManager>())
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

		m_timeManager->Startup();

		RenderManager::Get()->Startup();	// Initializes SDL events and video subsystems

		// For some reason, this needs to be called after the SDL video subsystem (!) has been initialized:
		InputManager::Get()->Startup();

		// Must wait to start scene manager and load a scene until the renderer is called, since we need to initialize
		// the rendering context in the RenderManager before creating shaders
		SceneManager::Get()->Startup();

		// Now that the scene (and its materials/shaders) has been loaded, we can initialize the shaders
		RenderManager::Get()->Initialize();

		m_isRunning = true;

		return;
	}


	// Main game loop
	void CoreEngine::Run()
	{
		LOG("CoreEngine beginning main game loop!");

		// Process any events that might have occurred during startup:
		EventManager::Get()->Update();
		m_logManager->Update();

		// Initialize game loop timing:
		m_timeManager->Update();
		double elapsed = 0.0;

		while (m_isRunning)
		{
			InputManager::Get()->Update();

			// Process events
			EventManager::Get()->Update(); // Clears SDL event queue: Must occur after any other component that listens to SDL events
			m_logManager->Update();

			m_timeManager->Update(); // We only need to call this once per loop. DeltaTime() effectively == #ms between calls to TimeManager.Update()
			elapsed += m_timeManager->DeltaTime();

			while (elapsed >= m_FixedTimeStep)
			{
				// Update components:
				EventManager::Get()->Update(); // Clears SDL event queue: Must occur after any other component that listens to SDL events
				m_logManager->Update();

				SceneManager::Get()->Update(); // Updates all of the scene objects

				elapsed -= m_FixedTimeStep;
			}
			
			RenderManager::Get()->Update();

			Update();
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
		m_timeManager->Shutdown();		
		InputManager::Get()->Shutdown();
		RenderManager::Get()->Shutdown();
		SceneManager::Get()->Shutdown();
		EventManager::Get()->Shutdown();
		m_logManager->Shutdown();

		return;
	}

	
	void CoreEngine::Update()
	{
		// Generate a quit event if the quit button is pressed:
		if (InputManager::Get()->GetKeyboardInputState(en::InputButton_Quit) == true)
		{
			EventManager::Get()->Notify(std::make_shared<en::EventManager::EventInfo const>(
					en::EventManager::EventInfo{en::EventManager::EngineQuit, this, "Core Engine Quit"}));
		}
	}


	void CoreEngine::HandleEvent(std::shared_ptr<en::EventManager::EventInfo const> eventInfo)
	{
		switch (eventInfo->m_type)
		{
		case en::EventManager::EngineQuit:
		{
			Stop();
		}			
			break;

		default:
			LOG_ERROR("ERROR: Default event generated in CoreEngine!");
			break;
		}

		return;
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

		bool foundSceneArg = false;

		for (int i = 1; i < argc; i++)
		{			
			const string currentArg(argv[i]);
			if (currentArg.find("-scene") != string::npos)
			{
				SEAssert("\"-scene\" argument specified more than once!", foundSceneArg == false);
				foundSceneArg = true;

				if (i < argc - 1) // -1 as we need to peek ahead
				{
					const int nextArg = i + 1;
					const string param = string(argv[nextArg]);

					LOG("\tReceived scene command: \"%s %s\"", currentArg.c_str(), param.c_str());

					const string scenesRoot = Config::Get()->GetValue<string>("scenesRoot"); // "..\Scenes\"

					// From param of the form "Scene\Folder\Names\sceneFile.extension", we extract:

					// sceneFilePath == "..\Scenes\Scene\Folder\Names\sceneFile.extension":
					const string sceneFilePath = scenesRoot + param; 
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
					return false;
				}
				
				i++; // Consume the token
			}
			else
			{
				LOG_ERROR("\"%s\" is not a recognized command!", currentArg.c_str());
			}

		}

		return true;
	}
}