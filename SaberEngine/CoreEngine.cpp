#include <string>

#include "CoreEngine.h"
#include "BuildConfiguration.h"
#include "enPlatform.h"
#include "rePlatform.h"


namespace SaberEngine
{
	// Static members:
	CoreEngine*		CoreEngine::coreEngine; // Assigned in constructor

	EventManager*	CoreEngine::SaberEventManager	= &EventManager::Instance();
	InputManager*	CoreEngine::SaberInputManager	= &InputManager::Instance();
	SceneManager*	CoreEngine::SaberSceneManager	= &SceneManager::Instance();
	RenderManager*	CoreEngine::SaberRenderManager	= &RenderManager::Instance();


	CoreEngine::CoreEngine(int argc, char** argv) : SaberObject("CoreEngine")
	{
		coreEngine = this;

		if (!ProcessCommandLineArgs(argc, argv))
		{
			exit(-1);
		}
	}


	void CoreEngine::Startup()
	{
		LOG("CoreEngine starting...");

		// Set non-rendering platform-specific bindings:
		if (!en::platform::ConfigureEnginePlatform())
		{
			LOG_ERROR("Failed to configure engine platform!");
			exit(-1);
		}

		LOG("Initializing rendering API...");

		// Set our API-specific bindings:
		if (!re::platform::RegisterPlatformFunctions())
		{
			LOG_ERROR("Failed to configure rendering API!");
			exit(-1);
		}

		// Initialize SaberEngine:
		SaberEventManager->Startup();	
		SaberLogManager->Startup();
		
		SaberEventManager->Subscribe(EVENT_ENGINE_QUIT, this);

		SaberTimeManager->Startup();
		SaberInputManager->Startup();

		SaberRenderManager->Startup();

		// Must wait to start scene manager and load a scene until the renderer is called, since we need to initialize OpenGL in the RenderManager before creating shaders
		SaberSceneManager->Startup();
		bool loadedScene = SaberSceneManager->LoadScene(m_config.m_currentScene);

		// Now that the scene (and its materials/shaders) has been loaded, we can initialize the shaders
		if (loadedScene)
		{
			SaberRenderManager->Initialize();
		}		

		m_isRunning = true;

		return;
	}


	// Main game loop
	void CoreEngine::Run()
	{
		LOG("CoreEngine beginning main game loop!");

		// Process any events that might have occurred during startup:
		SaberEventManager->Update();
		SaberLogManager->Update();

		// Initialize game loop timing:
		SaberTimeManager->Update();
		double elapsed = 0.0;

		while (m_isRunning)
		{
			SaberInputManager->Update();

			// Process events
			SaberEventManager->Update(); // Clears SDL event queue: Must occur after any other component that listens to SDL events
			SaberLogManager->Update();

			SaberTimeManager->Update();	// We only need to call this once per loop. DeltaTime() effectively == #ms between calls to TimeManager.Update()
			elapsed += SaberTimeManager->DeltaTime();

			while (elapsed >= FIXED_TIMESTEP)
			{
				// Update components:
				SaberEventManager->Update(); // Clears SDL event queue: Must occur after any other component that listens to SDL events
				SaberLogManager->Update();

				SaberSceneManager->Update(); // Updates all of the scene objects

				elapsed -= FIXED_TIMESTEP;
			}
			
			SaberRenderManager->Update();

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

		m_config.SaveConfig();
		
		// Note: Shutdown order matters!
		SaberTimeManager->Shutdown();		
		SaberInputManager->Shutdown();
		
		SaberRenderManager->Shutdown();
		
		SaberSceneManager->Shutdown();
		
		SaberEventManager->Shutdown();

		SaberLogManager->Shutdown();

		delete SaberTimeManager;
		delete SaberInputManager;
		delete SaberRenderManager;
		delete SaberSceneManager;
		delete SaberEventManager;
		delete SaberLogManager;		

		SDL_Quit();

		return;
	}


	EngineConfig const* CoreEngine::GetConfig()
	{
		return &m_config;
	}

	
	void CoreEngine::Update()
	{
		// Generate a quit event if the quit button is pressed:
		if (SaberInputManager->GetKeyboardInputState(INPUT_BUTTON_QUIT) == true)
		{
			SaberEventManager->Notify(new EventInfo{ EVENT_ENGINE_QUIT, this });
		}
	}


	void CoreEngine::HandleEvent(EventInfo const* eventInfo)
	{
		switch (eventInfo->m_type)
		{
		case EVENT_ENGINE_QUIT:
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
		LOG("Processing " + to_string(argc - 1) + " command line arguments:");

		for (int i = 1; i < argc; i++) // Start at index 1 to skip the executable path
		{			
			string currentArg = string(argv[i]);			
			if (currentArg.find("-scene") != string::npos)
			{
				string parameter = string(argv[i + 1]);
				if (i < argc - 1)
				{
					LOG("\tReceived scene command: \"" + currentArg + " " + parameter + "\"");

					m_config.m_currentScene = parameter;
				}
				
				i++; // Eat the extra command parameter
			}
			else
			{
				LOG_ERROR("\"" + currentArg + "\" is not a recognized command!");
			}

		}

		return true;
	}
}