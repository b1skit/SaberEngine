#include <string>

#include "CoreEngine.h"
#include "BuildConfiguration.h"
#include "Platform.h"


namespace SaberEngine
{
	// Static members:
	en::EngineConfig CoreEngine::m_config;

	CoreEngine*		CoreEngine::coreEngine			= nullptr;
	EventManager*	CoreEngine::SaberEventManager	= nullptr;
	InputManager*	CoreEngine::SaberInputManager	= nullptr;
	SceneManager*	CoreEngine::SaberSceneManager	= nullptr;
	RenderManager*	CoreEngine::SaberRenderManager	= nullptr;

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

		// Initialize manager singletons:
		SaberEventManager = &EventManager::Instance();
		SaberInputManager = &InputManager::Instance();
		SaberSceneManager = &SceneManager::Instance();
		SaberRenderManager = &RenderManager::Instance();

		// Start managers:
		SaberEventManager->Startup();	
		SaberLogManager->Startup();

		SaberEventManager->Subscribe(EVENT_ENGINE_QUIT, this);

		SaberTimeManager->Startup();

		SaberRenderManager->Startup();	// Initializes SDL events and video subsystems

		// For some reason, this needs to be called after the SDL video subsystem (!) has been initialized:
		SaberInputManager->Startup();

		// Must wait to start scene manager and load a scene until the renderer is called, since we need to initialize
		// OpenGL in the RenderManager before creating shaders
		SaberSceneManager->Startup();
		bool loadedScene = SaberSceneManager->LoadScene(m_config.SceneName());

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


	en::EngineConfig const* CoreEngine::GetConfig()
	{
		return &m_config;
	}

	
	void CoreEngine::Update()
	{
		// Generate a quit event if the quit button is pressed:
		if (SaberInputManager->GetKeyboardInputState(INPUT_BUTTON_QUIT) == true)
		{
			SaberEventManager->Notify(std::make_shared<EventInfo const>(EventInfo{ 
				EVENT_ENGINE_QUIT, 
				this,
				"Core Engine Quit"}));
		}
	}


	void CoreEngine::HandleEvent(std::shared_ptr<EventInfo const> eventInfo)
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
		const int numTokens = argc - 1; // -1, as 1st arg is program name
		LOG("Processing " + to_string(numTokens) + " command line tokens...");

		for (int i = 1; i < argc; i++)
		{			
			string currentArg = string(argv[i]);			
			if (currentArg.find("-scene") != string::npos)
			{
				if (i < argc - 1) // -1 as we need to peek ahead
				{
					const int nextArg = i + 1;
					const string parameter = string(argv[nextArg]);

					LOG("\tReceived scene command: \"" + currentArg + " " + parameter + "\"");

					m_config.SceneName() = parameter;
				}
				else
				{
					LOG_ERROR("Received \"-scene\" token, but no matching scene name");
					return false;
				}
				
				i++; // Eat the parameter
			}
			else
			{
				LOG_ERROR("\"" + currentArg + "\" is not a recognized command!");
			}

		}

		return true;
	}
}